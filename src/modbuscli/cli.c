#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <modbus.h>


int main()
{
        uint8_t tab_bits[32];
        uint16_t tab_reg[32];
        char buf[64], cmd[20];
        int slave, addr, nb, rc;
        modbus_t *ctx = NULL;
        while (1) {
                printf("> ");
                fflush(stdout);
                if (!fgets(buf, sizeof(buf), stdin))
                        break;
                if (*buf == '\n') continue;
                if (strncmp(buf, "tcp ", 4) == 0) {
                        char ip[20];
                        int port;
                        if (sscanf(buf + 4, "%19s %d", ip, &port) != 2) {
                                printf("%s\n", "parse error");
                                continue;
                        }
                        printf("ip = %s, port = %d\n", ip, port);
                        if (ctx) {
                                modbus_close(ctx);
                                modbus_free(ctx);
                                ctx = NULL;
                        }
                        if (!(ctx = modbus_new_tcp(ip, port))) {
                                printf("%s\n", modbus_strerror(errno));
                                continue;
                        }
                        modbus_set_debug(ctx, 1);
                        if (modbus_connect(ctx) == -1) {
                                modbus_free(ctx);
                                ctx = NULL;
                                printf("%s\n", modbus_strerror(errno));
                                continue;
                        }
                        continue;
                } else if (strncmp(buf, "rtu ", 4) == 0) {
                        char dev[256], mode[4] = "8E1";
                        int baud;
                        rc = sscanf(buf + 4, "%256s %d %4s", dev, &baud, mode);
                        if (rc != 3) {
                                printf("%s\n", "parse error");
                                continue;
                        }
                        int data = mode[0], stop = mode[2];
                        if (ctx) {
                                modbus_close(ctx);
                                modbus_free(ctx);
                                ctx = NULL;
                        }
                        ctx = modbus_new_rtu(dev, baud, mode[1], data, stop);
                        if (!ctx) {
                                printf("%s\n", modbus_strerror(errno));
                                continue;
                        }
                        modbus_set_debug(ctx, 1);
                        if (modbus_connect(ctx) == -1) {
                                modbus_free(ctx);
                                ctx = NULL;
                                printf("%s\n", modbus_strerror(errno));
                                continue;
                        }
                        continue;
                }
                if (!ctx) {
                        printf("%s\n", "no modbus tcp/rtu master found.");
                        continue;
                }
                rc = sscanf(buf, "%20s %d %d %d", cmd, &slave, &addr, &nb);
                if (rc < 4) {
                        printf("%s\n", "parse error");
                        continue;
                }
                if (modbus_set_slave(ctx, slave) == -1) {
                        printf("set_slave(): %s\n", modbus_strerror(errno));
                        continue;
                }
                if (strcmp(cmd, "read_co") == 0) {
                        rc = modbus_read_bits(ctx, addr, nb, tab_bits);
                        if (rc == -1) {
                                printf("%s\n", modbus_strerror(errno));
                        } else {
                                for (int i = 0; i < rc; ++i)
                                        printf("%d ", tab_bits[i]);
                                printf("%s", "\n");
                        }
                } else if (strcmp(cmd, "read_di") == 0) {
                        rc = modbus_read_input_bits(ctx, addr, nb, tab_bits);
                        if (rc == -1) {
                                printf("%s\n", modbus_strerror(errno));
                        } else {
                                for (int i = 0; i < rc; ++i)
                                        printf("%d ", tab_bits[i]);
                                printf("%s", "\n");
                        }
                } else if (strcmp(cmd, "read_hr") == 0) {
                        rc = modbus_read_registers(ctx, addr, nb, tab_reg);
                        if (rc == -1) {
                                printf("%s\n", modbus_strerror(errno));
                        } else {
                                for (int i = 0; i < rc; ++i)
                                        printf("%d ", tab_reg[i]);
                                printf("%s", "\n");
                        }
                } else if (strcmp(cmd, "read_ir") == 0) {
                        rc = modbus_read_input_registers(ctx,addr,nb,tab_reg);
                        if (rc == -1) {
                                printf("%s\n", modbus_strerror(errno));
                        } else {
                                for (int i = 0; i < rc; ++i)
                                        printf("%d ", tab_reg[i]);
                                printf("%s", "\n");
                        }
                } else if (strcmp(cmd, "write_co") == 0) {
                        rc = modbus_write_bit(ctx, addr, nb);
                        if (rc == -1) {
                                printf("%s\n", modbus_strerror(errno));
                        }
                } else if (strcmp(cmd, "write_hr") == 0) {
                        rc = modbus_write_register(ctx, addr, nb);
                        if (rc == -1) {
                                printf("%s\n", modbus_strerror(errno));
                        }
                } else {
                        printf("%s\n", "invalid command");
                }
        }
        modbus_close(ctx);
        modbus_free(ctx);
        return 0;
}
