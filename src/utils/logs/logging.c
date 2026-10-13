#include <utils/logs/logging.h>
#ifdef DEBUG
static char log_buffer[LOGGING_LOG_BUFFERF_MAX_SIZE];

#if LOGGING_ENABLE_BINARY_FORMAT

const char *nibbleToBin[16]
    = {"0000", "0001", "0010", "0011", "0100", "0101", "0110", "0111", "1000", "1001", "1010", "1011", "1100", "1101", "1110", "1111"};

// helper: convierte u16 en string binario
void u16ToBinaryStr(u16 value, char *buffer) {
    // Cada nibble ocupa 4 caracteres
    strcpy(buffer, nibbleToBin[(value >> 12) & 0xF]);
    strcat(buffer, nibbleToBin[(value >> 8) & 0xF]);
    strcat(buffer, nibbleToBin[(value >> 4) & 0xF]);
    strcat(buffer, nibbleToBin[value & 0xF]);
}

// logger extendido con %b
void LOGGING_print(const char *level, const char *fun, const char *fmt, ...) {
    va_list args;
    char tmp_buffer[LOGGING_LOG_BUFFERF_MAX_SIZE];
    char bin_buffer[32]; // para %b

    const char *p = fmt;
    char *out = tmp_buffer;
    va_start(args, fmt);

    while (*p) {
        if (*p == '%' && *(p + 1) == 'b') {
            // nuestro %b personalizado
            u16 val = (u16)va_arg(args, int); // ints menores se promocionan a int en varargs
            u16ToBinaryStr(val, bin_buffer);
            strcpy(out, bin_buffer);
            out += strlen(bin_buffer);
            p += 2; // saltar "%b"
        } else {
            // delegar a sprintf normal
            if (*p == '%') {
                // copiamos este % y el siguiente char para sprintf
                char fmt2[3] = {p[0], p[1], 0};
                char tmp[64];
                p++; // avanzar al especificador
                switch (*p) {
                case 'u':
                case 'd':
                case 'x':
                case 'X':
                case 'c':
                    sprintf(tmp, fmt2, va_arg(args, int));
                    break;
                case 's':
                    sprintf(tmp, fmt2, va_arg(args, char *));
                    break;
                default:
                    sprintf(tmp, "%%?");
                    break;
                }
                strcpy(out, tmp);
                out += strlen(tmp);
                p++;
            } else {
                *out++ = *p++;
            }
        }
    }
    *out = '\0';
    va_end(args);

    // finalmente mandamos al buffer global
    sprintf(log_buffer, "[%s][%s] %s", level, fun, tmp_buffer);
    LOGGING_WRITER_NG("%s", log_buffer);
}
#else

void LOGGING_print(const char *level, const char *fun, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_end(args);
    vsprintf(log_buffer, fmt, args);
    LOGGING_WRITER_NG("[%s][%s] %s", level, fun, log_buffer);
}
#endif

void LOGGING_printMatrix(const char *level, const char *fun, const char *fmt, void *mtx, u16 w, u16 size) {
    LOGGING_WRITER_NG("[%s][%s]", level, fun);
    memset(log_buffer, 0, LOGGING_LOG_BUFFERF_MAX_SIZE);
    char elem_buffer[32];
    for (u16 i = 0; i < size; i++) {
        sprintf(elem_buffer, fmt, ((u8 *)mtx)[i]);
        strcat(log_buffer, elem_buffer);
        if ((i + 1) % w == 0) {
            LOGGING_WRITER_NG("%s", log_buffer);
            memset(log_buffer, 0, LOGGING_LOG_BUFFERF_MAX_SIZE);
        } else {
            strcat(log_buffer, " ");
        }
    }
}
#endif