#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <modbus.h>
#define SLAVE_NSIZE 248
#define MODBUS_NSIZE 16
#define SOCKET_NSIZE 128
#define MODBUS_TCP_HEADER_LENGTH 7
#define MODBUS_RTU_HEADER_LENGTH 1
#define _max(x, y) (x >= y ? x : y)
#define _min(x, y) (x <= y ? x : y)
static char run = 1;


typedef struct mb_session_t
{
        modbus_t *ctx[MODBUS_NSIZE];
        modbus_mapping_t *map[MODBUS_NSIZE][SLAVE_NSIZE];
        char *dev[MODBUS_NSIZE];
        int sock[SOCKET_NSIZE];
        short sosize;
        int lisock;
        int fdmax;
        fd_set rdfds;
        fd_set wrfds;
} mb_session_t;


static int slave_to_index(int slave, int istcp)
{
        if (slave > 0 && slave <= 247) return slave;
        else if (istcp && slave == 255) return 0;
        else return -1;
}


static int mb_rtu_new(mb_session_t *sp, const char *dev, int baud,
                      const char *mode)
{
        int i = 1;
        for (; i < MODBUS_NSIZE && sp->ctx[i]; ++i) {
                if (strcmp(dev, sp->dev[i]) == 0)
                        return i;
        }
        if (i >= MODBUS_NSIZE) {
                printf("rtu_new(): %s\n", strerror(ENOMEM));
                return -1;
        }
        modbus_t *ctx = modbus_new_rtu(dev, baud, mode[1], mode[0], mode[2]);
        if (!ctx) {
                printf("rtu_new(): %s\n", modbus_strerror(errno));
                return -1;
        }
        if (modbus_connect(ctx) == -1) {
                printf("connect(): %s\n", modbus_strerror(errno));
                modbus_free(ctx);
                return -1;
        }
        const int fd = modbus_get_socket(ctx);
        sp->ctx[i] = ctx;
        sp->dev[i] = (char*)malloc(strlen(dev) + 1);
        strcpy(sp->dev[i], dev);
        sp->fdmax = _max(sp->fdmax, fd);
        FD_SET(fd, &sp->rdfds);
        modbus_set_debug(ctx, 1);
        return i;
}


static int mb_tcp_new(mb_session_t *sp, int port)
{
        printf("%s\n", "tcp_new()...");
        if (sp->ctx[0]) return 0;
        modbus_t *ctx = modbus_new_tcp(NULL, port);
        if (!ctx) {
                printf("tcp_new(): %s\n", modbus_strerror(errno));
                return -1;
        }
        const int sock = modbus_tcp_listen(ctx, 5);
        if (sock == -1) {
                modbus_free(ctx);
                printf("tcp_listen(): %s\n", modbus_strerror(errno));
                return -1;
        }
        sp->ctx[0] = ctx;
        sp->lisock = sock;
        sp->fdmax = _max(sp->fdmax, sock);
        FD_SET(sock, &sp->rdfds);
        modbus_set_debug(ctx, 1);
        return 0;
}


static int mb_socket_new(mb_session_t *sp)
{
        printf("socket_new()...\n");
        if (sp->sosize >= SOCKET_NSIZE) {
                printf("socket_new(): %s\n", strerror(ENOMEM));
                return -1;
        }
        struct sockaddr_in addr;
        socklen_t len = sizeof(struct sockaddr_in);
        const int sock = accept(sp->lisock, (struct sockaddr*)&addr, &len);
        if (sock == -1) {
                printf("accept(): %s\n", strerror(errno));
                return -1;
        }
        sp->sock[sp->sosize++] = sock;
        sp->fdmax = _max(sp->fdmax, sock);
        FD_SET(sock, &sp->rdfds);
        return sock;
}


static void mb_socket_free(mb_session_t *sp, int soidx)
{
        close(sp->sock[soidx]);
        FD_CLR(sp->sock[soidx], &sp->rdfds);
        if (sp->sock[soidx] == sp->fdmax) --sp->fdmax;
        memmove(sp->sock+soidx, sp->sock+soidx+1, sp->sosize-soidx-1);
        --sp->sosize;
}


static int mb_map_new(mb_session_t *sp, int idx, int slave, int co, int di,
                      int hr, int ir)
{
        if (idx >= MODBUS_NSIZE) {
                printf("idx: %s\n", strerror(EINVAL));
                return -1;
        }
        if ((slave = slave_to_index(slave, idx == 0)) == -1) {
                printf("slave: %s\n", strerror(EINVAL));
                return -1;
        }
        modbus_mapping_t *map = modbus_mapping_new(co, di, hr, ir);
        if (!map) {
                printf("map_new(): %s\n", modbus_strerror(errno));
                return -1;
        }
        sp->map[idx][slave] = map;
        return slave;
}


static void mb_map_free(mb_session_t *sp, int idx, int slave)
{
        if ((slave = slave_to_index(slave, idx == 0)) == -1) return;
        modbus_mapping_free(sp->map[idx][slave]);
        sp->map[idx][slave] = NULL;
}


static mb_session_t* mb_session_new()
{
        mb_session_t *sp = (mb_session_t*)malloc(sizeof(mb_session_t));
        if (!sp) {
                printf("session_new(): %s\n", strerror(errno));
                return NULL;
        }
        memset(sp->ctx, 0, sizeof(sp->ctx));
        memset(sp->map, 0, sizeof(sp->map));
        memset(sp->dev, 0, sizeof(sp->dev));
        for (int i = 0; i < SOCKET_NSIZE; ++i)
                sp->sock[i] = -1;
        sp->sosize = 0;
        sp->lisock = sp->fdmax = -1;
        FD_ZERO(&sp->rdfds);
        FD_ZERO(&sp->wrfds);
        return sp;
}


static void mb_session_free(mb_session_t *sp)
{
        for (int i = 0; i < MODBUS_NSIZE && sp->ctx[i]; ++i) {
                modbus_free(sp->ctx[i]);
                free(sp->dev);
                for (int j = 0; j < SLAVE_NSIZE; ++j) {
                        if (sp->map[i]) {
                                modbus_mapping_free(sp->map[i][j]);
                        }
                }
        }
        free(sp);
}


static int mb_map_set(mb_session_t *sp, int idx, int slave, int type,
                      uint16_t addr, uint16_t value)
{
        if (idx >= MODBUS_NSIZE) {
                printf("idx: %s\n", strerror(EINVAL));
                return -1;
        }
        if ((slave = slave_to_index(slave, idx == 0)) == -1) {
                printf("slave: %s\n", strerror(EINVAL));
                return -1;
        }
        modbus_mapping_t *map = sp->map[idx][slave];
        if (!map) return -1;
        if (type == 0 && addr < map->nb_bits) {
                map->tab_bits[addr] = value ? 1 : 0;
        } else if (type == 1 && addr < map->nb_input_bits) {
                map->tab_input_bits[addr] = value ? 1 : 0;
        } else if (type == 2 && addr < map->nb_registers) {
                map->tab_registers[addr] = value;
        } else if (type == 3 && addr < map->nb_input_registers) {
                map->tab_input_registers[addr] = value;
        } else {
                printf("map_set(): %s\n", strerror(EINVAL));
                return -1;
        }
        return 0;
}


static void print_mb_session(mb_session_t *sp)
{
        if (sp->ctx[0])
                printf("%d: %s\n", 0, "0.0.0.0");
        for (int i = 1; i < MODBUS_NSIZE; ++i) {
                if (!sp->ctx[i]) continue;
                printf("%d: %s\n", i, sp->dev[i]);
        }
}


static void print_mb_ctx(mb_session_t *sp, int idx)
{
        if (idx >= MODBUS_NSIZE) {
                printf("idx: %s\n", strerror(EINVAL));
                return;
        }
        for (int i = 1; i < SLAVE_NSIZE; ++i) {
                if (!sp->map[idx][i]) continue;
                modbus_mapping_t *map = sp->map[idx][i];
                printf("%d (%d %d %d %d)\n", i, map->nb_bits,
                       map->nb_input_bits, map->nb_registers,
                       map->nb_input_registers);
        }
        if (idx == 0 && sp->map[idx][0]) {
                modbus_mapping_t *map = sp->map[idx][0];
                printf("%d (%d %d %d %d)\n", 255, map->nb_bits,
                       map->nb_input_bits, map->nb_registers,
                       map->nb_input_registers);
        }
}


static void print_mb_map(mb_session_t *sp, int idx, int slave, uint16_t start,
                         uint16_t end)
{
        if (idx >= MODBUS_NSIZE) {
                printf("idx: %s\n", strerror(EINVAL));
                return;
        }
        if ((slave = slave_to_index(slave, idx == 0)) == -1) {
                printf("slave: %s\n", modbus_strerror(EINVAL));
                return;
        }
        modbus_mapping_t *map = sp->map[idx][slave];
        if (!map) return;
        uint16_t a = _max(0, start);
        uint16_t b = _max(map->nb_bits, map->nb_input_bits);
        b = _max(b, _max(map->nb_registers, map->nb_input_registers));
        b = _min(b, end);
        if (a > b) {
                int temp = a;
                a = b;
                b = temp;
        }
        for (int i = a; i < b; ++i) {
                printf("%d ", i);
                if (i < map->nb_bits)
                        printf("%d ", map->tab_bits[i]);
                else
                        printf("  ");
                if (i < map->nb_input_bits)
                        printf("%d ", map->tab_input_bits[i]);
                else
                        printf("  ");
                if (i < map->nb_registers)
                        printf("%04x ", map->tab_registers[i]);
                else
                        printf("     ");
                if (i < map->nb_input_registers)
                        printf("%04x", map->tab_input_registers[i]);
                printf("\n");
        }
}


static void cmd(mb_session_t *sp, const char *buf, size_t len)
{
        const char *pch = buf;
        while (isspace(*pch)) ++pch;
        if (strncmp(pch, "tcp ", 4) == 0) {
                int port, idx, slave, co, di, hr, ir;
                if (sscanf(pch + 4, "%d %d %d %d %d %d", &port,
                           &slave, &co, &di, &hr, &ir) != 6) {
                        printf("%s\n", strerror(EINVAL));
                        return;
                }
                if ((idx = mb_tcp_new(sp, port)) == -1)
                        return;
                printf("%d %d %d %d %d %d\n", port, slave, co, di, hr, ir);
                if (mb_map_new(sp, idx, slave, co, di, hr, ir) == -1)
                        mb_map_free(sp, idx, slave);
        } else if (strncmp(pch, "rtu ", 4) == 0) {
                pch += 4;
                while (isspace(*pch)) ++pch;
                const char *dev = pch;
                while (!isspace(*pch)) ++pch;
                while (isspace(*pch)) ++pch;
                const char *mode = pch;
                int slave, baud, idx, co, di, hr, ir;
                if (sscanf(pch, "%d %d %d %d %d %d", &baud, &slave,
                           &co, &di, &hr, &ir) != 6) {
                        printf("%s\n", strerror(EINVAL));
                        return;
                }
                if ((idx = mb_rtu_new(sp, dev, baud, mode)) == -1)
                        return;
                if (mb_map_new(sp, idx, slave, co, di, hr, ir) == -1)
                        mb_map_free(sp, idx, slave);
        } else if (strncmp(pch, "set ", 4) == 0) {
                int idx, slave, type, addr, value;
                if (sscanf(pch + 4, "%d %d %d %d %d", &idx, &slave,
                           &type, &addr, &value) != 5) {
                        printf("%s\n", strerror(EINVAL));
                        return;
                }
                mb_map_set(sp, idx, slave, type, addr, value);
        } else if (strncmp(pch, "print", 5) == 0) {
                int idx, slave, start, end;
                int rc = sscanf(pch + 5, "%d %d %d %d", &idx, &slave,
                                &start, &end);
                if (rc == 0 || rc == EOF)
                        print_mb_session(sp);
                else if (rc == 1)
                        print_mb_ctx(sp, idx);
                else if (rc == 2)
                        print_mb_map(sp, idx, slave, 0, -1);
                else if (rc == 4)
                        print_mb_map(sp, idx, slave, start, end);
                else
                        printf("%s\n", strerror(EINVAL));

        } else if (len > 1) {
                printf("%s\n", "invalid command");
        } else if (!len) {
                exit(0);
        }
}


static int handle_tcp(mb_session_t *sp, int soidx)
{
        uint8_t req[MODBUS_TCP_MAX_ADU_LENGTH];
        modbus_t *ctx = sp->ctx[0];
        modbus_set_socket(ctx, sp->sock[soidx]);
        int len = modbus_receive(ctx, req);
        if (len <= 0) return len;
        int slave = req[MODBUS_TCP_HEADER_LENGTH - 1];
        modbus_mapping_t *map = sp->map[0][slave_to_index(slave, 1)];
        if (!map) return 0;
        modbus_set_slave(ctx, slave);
        if (modbus_reply(ctx, req, len, map) <= 0) {
                printf("reply(): %s\n", modbus_strerror(errno));
                return -1;
        }
        return 0;
}


static int handle_rtu(mb_session_t *sp, int idx)
{
        uint8_t req[MODBUS_RTU_MAX_ADU_LENGTH];
        modbus_t *ctx = sp->ctx[idx];
        int len = modbus_receive(ctx, req);
        if (len <= 0) return len;
        const int slave = req[MODBUS_RTU_HEADER_LENGTH - 1];
        modbus_mapping_t *map = sp->map[idx][slave_to_index(slave, 0)];
        if (!map) return 0;
        modbus_set_slave(ctx, slave);
        if (modbus_reply(ctx, req, len, map) <= 0) {
                printf("reply(): %s\n", modbus_strerror(errno));
                return -1;
        }
        return 0;
}


int main()
{
        mb_session_t *sp = mb_session_new();
        if (!sp) return 0;
        FD_SET(STDIN_FILENO, &sp->rdfds);
        sp->fdmax = STDIN_FILENO;
        while (run) {
                printf("> ");
                fflush(stdout);
                fd_set rdset = sp->rdfds, wrset = sp->wrfds;
                select(sp->fdmax + 1, &rdset, &wrset, NULL, NULL);
                /* user command */
                if (FD_ISSET(STDIN_FILENO, &rdset)) {
                        char buf[256];
                        int c = read(STDIN_FILENO, buf, sizeof(buf) - 1);
                        buf[c] = 0;
                        cmd(sp, buf, c);
                }
                /* new tcp client */
                if (sp->lisock != -1 && FD_ISSET(sp->lisock, &rdset)) {
                        mb_socket_new(sp);
                }
                /* modbus tcp request */
                for (int i = 0; i < sp->sosize; ++i) {
                        if (!FD_ISSET(sp->sock[i], &rdset))
                                continue;
                        if (handle_tcp(sp, i) == -1)
                                mb_socket_free(sp, i);
                }
                /* modbus rtu request */
                for (int i = 1; i < MODBUS_NSIZE && sp->ctx[i]; ++i) {
                        if (!FD_ISSET(modbus_get_socket(sp->ctx[i]), &rdset))
                                continue;
                        handle_rtu(sp, i);
                }
        }
        mb_session_free(sp);
        return 0;
}
