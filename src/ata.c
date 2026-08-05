#include "kernel.h"

#define ATA_PRIMARY_IO 0x1F0
#define ATA_PRIMARY_CTRL 0x3F6

#define ATA_REG_DATA (ATA_PRIMARY_IO + 0)
#define ATA_REG_SECCOUNT (ATA_PRIMARY_IO + 2)
#define ATA_REG_LBA0 (ATA_PRIMARY_IO + 3)
#define ATA_REG_LBA1 (ATA_PRIMARY_IO + 4)
#define ATA_REG_LBA2 (ATA_PRIMARY_IO + 5)
#define ATA_REG_HDDEVSEL (ATA_PRIMARY_IO + 6)
#define ATA_REG_COMMAND (ATA_PRIMARY_IO + 7)
#define ATA_REG_STATUS (ATA_PRIMARY_IO + 7)
#define ATA_REG_ALTSTATUS (ATA_PRIMARY_CTRL + 0)

#define ATA_CMD_READ_SECTORS 0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY 0xEC

#define ATA_STATUS_ERR 0x01
#define ATA_STATUS_DF 0x20
#define ATA_STATUS_DRQ 0x08
#define ATA_STATUS_BSY 0x80

#define ATA_TIMEOUT_ITERS 200000u

static void ata_delay_400ns(void) {
    inb(ATA_REG_ALTSTATUS);
    inb(ATA_REG_ALTSTATUS);
    inb(ATA_REG_ALTSTATUS);
    inb(ATA_REG_ALTSTATUS);
}

static int ata_wait_not_busy(void) {
    for (unsigned int i = 0; i < ATA_TIMEOUT_ITERS; i++) {
        uint8_t status = inb(ATA_REG_STATUS);
        if ((status & ATA_STATUS_BSY) == 0) {
            return 1;
        }
    }
    return 0;
}

static int ata_wait_drq_or_error(void) {
    for (unsigned int i = 0; i < ATA_TIMEOUT_ITERS; i++) {
        uint8_t status = inb(ATA_REG_STATUS);
        if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) {
            return 0;
        }
        if (status & ATA_STATUS_DRQ) {
            return 1;
        }
    }
    return 0;
}

static int ata_wait_ready_no_error(void) {
    if (!ata_wait_not_busy()) {
        return 0;
    }
    uint8_t status = inb(ATA_REG_STATUS);
    if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) {
        return 0;
    }
    return 1;
}

static void ata_select_drive(uint32_t lba) {
    outb(ATA_REG_HDDEVSEL, (uint8_t)(0xE0u | ((lba >> 24) & 0x0Fu)));
    ata_delay_400ns();
}

static void ata_soft_reset(void) {
    outb(ATA_REG_ALTSTATUS, 0x04);
    ata_delay_400ns();
    outb(ATA_REG_ALTSTATUS, 0x00);
    ata_delay_400ns();
    ata_wait_not_busy();
}

static int ata_read_sector_single(uint32_t lba, void *buffer) {
    uint16_t *data = (uint16_t *)buffer;

    ata_select_drive(lba);

    if (!ata_wait_not_busy()) {
        return 0;
    }

    outb(ATA_REG_SECCOUNT, 1);
    outb(ATA_REG_LBA0, (uint8_t)(lba & 0xFFu));
    outb(ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFFu));
    outb(ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFFu));
    outb(ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);

    if (!ata_wait_drq_or_error()) {
        return 0;
    }

    for (int i = 0; i < 256; i++) {
        __asm__ volatile("inw %w1, %w0" : "=a"(data[i]) : "Nd"(ATA_REG_DATA) : "memory");
    }

    return 1;
}

int ata_read_sector(uint32_t lba, void *buffer) {
    for (int attempt = 0; attempt < 3; attempt++) {
        if (ata_read_sector_single(lba, buffer)) {
            return 1;
        }
        ata_soft_reset();
    }
    return 0;
}

static int ata_write_sector_single(uint32_t lba, const void *buffer) {
    const uint16_t *data = (const uint16_t *)buffer;

    ata_select_drive(lba);

    if (!ata_wait_not_busy()) {
        return 0;
    }

    outb(ATA_REG_SECCOUNT, 1);
    outb(ATA_REG_LBA0, (uint8_t)(lba & 0xFFu));
    outb(ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFFu));
    outb(ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFFu));
    outb(ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);

    if (!ata_wait_drq_or_error()) {
        return 0;
    }

    for (int i = 0; i < 256; i++) {
        __asm__ volatile("outw %w0, %w1" : : "a"(data[i]), "Nd"(ATA_REG_DATA) : "memory");
    }

    if (!ata_wait_ready_no_error()) {
        return 0;
    }

    outb(ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    return ata_wait_ready_no_error();
}

int ata_write_sector(uint32_t lba, const void *buffer) {
    for (int attempt = 0; attempt < 3; attempt++) {
        if (ata_write_sector_single(lba, buffer)) {
            return 1;
        }
        ata_soft_reset();
    }
    return 0;
}

int ata_zero_sector(uint32_t lba) {
    uint8_t zeros[512] = {0};
    return ata_write_sector(lba, zeros);
}

uint32_t ata_get_sector_count(void) {
    uint16_t buf[256];

    outb(ATA_REG_HDDEVSEL, 0xA0);
    ata_delay_400ns();

    if (!ata_wait_not_busy()) {
        return 0;
    }

    outb(ATA_REG_SECCOUNT, 0);
    outb(ATA_REG_LBA0, 0);
    outb(ATA_REG_LBA1, 0);
    outb(ATA_REG_LBA2, 0);

    outb(ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    if (inb(ATA_REG_STATUS) == 0) {
        return 0;
    }

    if (!ata_wait_drq_or_error()) {
        return 0;
    }

    for (int i = 0; i < 256; i++) {
        __asm__ volatile("inw %w1, %w0" : "=a"(buf[i]) : "Nd"(ATA_REG_DATA) : "memory");
    }

    uint32_t sectors = buf[60] | ((uint32_t)buf[61] << 16);
    if (buf[83] & (1 << 10)) {
        sectors = buf[100] | ((uint32_t)buf[101] << 16);
    }

    return sectors;
}
