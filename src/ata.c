#include "kernel.h"

#define ATA_PRIMARY_IO 0x1F0
#define ATA_REG_DATA (ATA_PRIMARY_IO + 0)
#define ATA_REG_SECCOUNT (ATA_PRIMARY_IO + 2)
#define ATA_REG_LBA0 (ATA_PRIMARY_IO + 3)
#define ATA_REG_LBA1 (ATA_PRIMARY_IO + 4)
#define ATA_REG_LBA2 (ATA_PRIMARY_IO + 5)
#define ATA_REG_HDDEVSEL (ATA_PRIMARY_IO + 6)
#define ATA_REG_COMMAND (ATA_PRIMARY_IO + 7)
#define ATA_REG_STATUS (ATA_PRIMARY_IO + 7)

#define ATA_CMD_READ_SECTORS 0x20
#define ATA_CMD_WRITE_SECTORS 0x30

#define ATA_STATUS_ERR 0x01
#define ATA_STATUS_DF 0x20
#define ATA_STATUS_DRQ 0x08
#define ATA_STATUS_BSY 0x80

static int ata_wait_not_busy(void) {
    for (unsigned int i = 0; i < 200000u; i++) {
        uint8_t status = inb(ATA_REG_STATUS);
        if ((status & ATA_STATUS_BSY) == 0) {
            return 1;
        }
    }
    return 0;
}

static int ata_wait_drq_or_error(void) {
    for (unsigned int i = 0; i < 200000u; i++) {
        uint8_t status = inb(ATA_REG_STATUS);
        if (status & ATA_STATUS_ERR) {
            return 0;
        }
        if (status & ATA_STATUS_DF) {
            return 0;
        }
        if (status & ATA_STATUS_DRQ) {
            return 1;
        }
    }
    return 0;
}

int ata_read_sector(uint32_t lba, void *buffer) {
    uint16_t *data = (uint16_t *)buffer;

    if (!ata_wait_not_busy()) {
        return 0;
    }

    outb(ATA_REG_HDDEVSEL, (uint8_t)(0xE0u | ((lba >> 24) & 0x0Fu)));
    outb(ATA_REG_SECCOUNT, 1);
    outb(ATA_REG_LBA0, (uint8_t)(lba & 0xFFu));
    outb(ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFFu));
    outb(ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFFu));
    outb(ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);

    if (!ata_wait_drq_or_error()) {
        return 0;
    }

    for (int i = 0; i < 256; i++) {
        __asm__ volatile("inw %w1, %w0" : "=a"(data[i]) : "Nd"(ATA_REG_DATA): "memory");
    }

    return 1;
}

int ata_write_sector(uint32_t lba, const void *buffer) {
    const uint16_t *data = (const uint16_t *)buffer;

    if (!ata_wait_not_busy()) {
        return 0;
    }

    outb(ATA_REG_HDDEVSEL, (uint8_t)(0xE0u | ((lba >> 24) & 0x0Fu)));
    outb(ATA_REG_SECCOUNT, 1);
    outb(ATA_REG_LBA0, (uint8_t)(lba & 0xFFu));
    outb(ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFFu));
    outb(ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFFu));
    outb(ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);

    if (!ata_wait_drq_or_error()) {
        return 0;
    }

    for (int i = 0; i < 256; i++) {
        __asm__ volatile("outw %w0, %w1" : : "a"(data[i]), "Nd"(ATA_REG_DATA): "memory");
    }

    io_wait();
    io_wait();
    io_wait();
    io_wait();

    return ata_wait_not_busy();
}

int ata_zero_sector(uint32_t lba) {
	uint8_t sector[512] = {0};
	return ata_write_sector(lba, sector);
}

uint32_t ata_get_sector_count() {
    uint16_t buf[256];
    ata_wait_not_busy();
    outb(ATA_REG_HDDEVSEL, 0xA0); // Master Drive
    outb(ATA_REG_COMMAND, 0xEC);  // IDENTIFY
    ata_wait_not_busy();

    for (int i = 0; i < 256; i++) {
        __asm__ volatile("inw %w1, %w0" : "=a"(buf[i]) : "Nd"(ATA_REG_DATA) : "memory");
    }

    uint32_t sectors = buf[60] | ((uint32_t)buf[61] << 16);
    if (buf[83] & (1 << 10)) {
        sectors = buf[100] | ((uint32_t)buf[101] << 16);
    }

    return sectors;
}
