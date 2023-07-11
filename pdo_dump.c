#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define B(x, n) (((x) & (1U << (n))) != 0)
#define Y(x, n) (B((x), (n)) ? "YES" : "NO")
#define BITS(x,hi,lo) (((x) >> (lo)) & ((2U << ((hi)-(lo))) -  1U))

static const char * peak[4] = {
    "No overload (or see extended Source caps)",
    "Overload 1ms:150%, 2ms:125%, 10ms:110%",
    "Overload 1ms:200%, 2ms:150%, 10ms:125%",
    "Overload 1ms:200%, 2ms:175%, 10ms:150%"
};

int
main(int argc, char * argv[])
{
    const char * const defname = "/proc/device-tree/chosen/power/usbpd_power_data_objects";
    const char * name = argc >= 2 ? argv[1] : defname;
    FILE * f;
    unsigned int i;
    unsigned int last_v = 0;

    if (argc > 2 || strcmp(name, "-h") == 0)
    {
        printf("Usage: %s [<filename>]\n", argv[0]);
        printf("Default filename = '%s'\n", defname);
        printf("Filename = - reads stdin\n");
        return 1;
    }

    f = strcmp(name, "-") == 0 ? stdin : fopen(name, "rb");
    if (!f)
    {
        fprintf(stderr, "Failed to open '%s': %s\n", name, strerror(errno));
        exit(1);
    }

    for (i = 0; !feof(f); ++i)
    {
        uint32_t pdo = 0;
        uint8_t buf[4];

        if (fread(buf, 4, 1, f) != 1)
        {
            if (ferror(f))
            {
                fprintf(stderr, "Read error: %s\n", strerror(errno));
                exit(1);
            }
            break;
        }

        pdo = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

        printf("%d: %#08x:\n", i, pdo);
        switch (pdo >> 30)
        {
            case 0:
            {
                unsigned int p = BITS(pdo, 21, 20);
                unsigned int v = BITS(pdo, 19, 10) * 50;
                unsigned int a = BITS(pdo,  9,  0) * 10;
                printf("Fixed supply\n");
                if (i == 0 && v != 5000)
                    printf("*** PDO object 0 not 5V!\n");
                if (i != 0) {
                    if (last_v >= v)
                        printf("*** Voltages after object 0 not ascending\n");
                    last_v = v;
                }
                if (i == 0 || BITS(pdo, 29, 22) != 0)
                {
                    if (i != 0)
                        printf("*** B29..22 expected zero after PDO object 0\n");
                    printf("  Dual-Role Power: %s\n", Y(pdo, 29));
                    printf("  USB Suspend Supported: %s\n", Y(pdo, 28));
                    printf("  Unconstrained Power: %s\n", Y(pdo, 27));
                    printf("  USB Communications Capable: %s\n", Y(pdo, 26));
                    printf("  Dual-Role Data: %s\n", Y(pdo, 25));
                    printf("  Unchunked Extended Messages Supported: %s\n", Y(pdo, 24));
                    printf("  EPR Mode Capable: %s\n", Y(pdo, 23));
                    if (B(pdo, 22))
                        printf("  *** Reserved – Shall be set to zero [B22].\n");
                }
                printf("  Peak Current (%d): %s\n", p, peak[p]);
                printf("  Voltage: %2d.%03dV\n", v / 1000, v % 1000);
                printf("  Current: %2d.%03dA\n", a / 1000, a % 1000);
                break;
            }
            case 1:
            {
                unsigned int maxv = BITS(pdo, 29, 20) * 50;
                unsigned int minv = BITS(pdo, 19, 10) * 50;
                unsigned int w =    BITS(pdo,  9,  0) * 250;
                printf("Battery\n");
                printf("  Max Voltage: %2d.%03dV\n", maxv / 1000, maxv % 1000);
                printf("  Min Voltage: %2d.%03dV\n", minv / 1000, minv % 1000);
                printf("  Max Power:  %3d.%03dW\n", w / 1000, w % 1000);
                break;
            }
            case 2:
            {
                unsigned int maxv = BITS(pdo, 29, 20) * 50;
                unsigned int minv = BITS(pdo, 19, 10) * 50;
                unsigned int a =    BITS(pdo,  9,  0) * 10;
                printf("Variable Supply (non-Battery)\n");
                printf("  Max Voltage: %2d.%03dV\n", maxv / 1000, maxv % 1000);
                printf("  Min Voltage: %2d.%03dV\n", minv / 1000, minv % 1000);
                printf("  Max Current: %2d.%03dA\n", a / 1000, a % 1000);
                break;
            }
            case 3:
            {
                unsigned int apdo = (pdo >> 28) & 3;
                printf("Augmented Power Data Object (APDO) %d\n", apdo);
                switch (apdo)
                {
                    case 0:
                    {
                        unsigned int maxv = BITS(pdo, 24, 17) * 100;
                        unsigned int minv = BITS(pdo, 15,  8) * 100;
                        unsigned int a =    BITS(pdo,  6,  0) * 50;
                        printf("SPR Programmable Power Supply\n");
                        printf("  PPS Power limited: %s\n", Y(pdo, 27));
                        if (BITS(pdo, 26, 25))
                           printf("  *** Reserved – Shall be set to zero [B26..25]\n");
                        printf("  Max Voltage: %2d.%03dV\n", maxv / 1000, maxv % 1000);
                        if (B(pdo, 16))
                            printf("  *** Reserved – Shall be set to zero [B16]\n");
                        printf("  Min Voltage: %2d.%03dV\n", minv / 1000, minv % 1000);
                        if (B(pdo, 7))
                            printf("  *** Reserved – Shall be set to zero [B7]\n");
                        printf("  Max Current: %2d.%03dA\n", a / 1000, a % 1000);
                        break;
                    }
                    case 1:
                    {
                        unsigned int p =    BITS(pdo, 27, 26);
                        unsigned int maxv = BITS(pdo, 25, 17) * 100;
                        unsigned int minv = BITS(pdo, 15,  8) * 100;
                        unsigned int pdp =  BITS(pdo,  7,  0);
                        printf("EPR Adjustable Voltage Supply\n");
                        printf("  Peak Current (%d): %s\n", p, peak[p]);
                        printf("  Max Voltage: %2d.%03dV\n", maxv / 1000, maxv % 1000);
                        if (B(pdo, 16))
                            printf("  *** Reserved – Shall be set to zero [B16]\n");
                        printf("  Min Voltage: %2d.%03dV\n", minv / 1000, minv % 1000);
                        if (B(pdo, 7))
                            printf("Reserved – Shall be set to zero [B7]\n");
                        printf("  PDP: %3dW\n", pdp);
                        break;
                    }
                    default:
                        printf("Unhandled APDO type: %d\n", apdo);
                        break;
                }
                break;
            }
        }
    }

    fclose(f);
    return 0;
}

