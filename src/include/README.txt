C library functions in this library:

NOT IMPLEMENTED (or stubbed)           IMPLEMENTED
----------------------------           -----------

(from assert.h)                        (from assert.h)
                                       assert

(from ctype.h)                         (from ctype.h)
                                       isalnum
                                       isalpha
                                       isascii
                                       isblank
                                       iscntrl
                                       isdigit
                                       isgraph
                                       islower
                                       isprint
                                       ispunct
                                       isspace
                                       isupper
                                       isxdigit
				       tolower
				       toupper

(from locale.h)                        (from locale.h)
localeconv
setlocale

(from math.h)                          (from math.h)
acos                                   ceil
asin                                   fabs
atan                                   floor
atan2                                  fmod
cos                                    modf
cosh                                   pow
exp
frexp
ldexp
log
log10
sin
sinh
sqrt
tan
tanh

(from setjmp.h)                        (from setjmp.h)
longjmp
setjmp

(from signal.h)                        (from signal.h)
raise
signal

(from stdarg.h)                        (from stdarg.h)
                                       va_start
                                       va_arg
                                       va_end

(from stdio.h)                         (from stdio.h)
clearerr                               fgetpos
fclose                                 fseek 
feof                                   fsetpos 
ferror                                 ftell 
fflush                                 getc 
fgetc                                  getchar 
fgets                                  gets 
fopen                                  perror 
fprintf                                printf
fputc                                  putc 
fputs                                  putchar 
fread                                  puts 
freopen                                remove 
fscanf                                 rename 
fwrite                                 rewind 
scanf                                  sprintf
setbuf
setvbuf
sscanf
tmpfile
tmpnam
ungetc
vfprintf
vprintf
vsprintf

(from stdlib.h)                        (from stdlib.h)
abort                                  abs
atexit                                 atoi
atof                                   calloc
atol                                   div
bsearch                                exit
getenv                                 free (sort of)
mblen                                  labs 
mbstowcs                               ldiv 
mbtowc                                 malloc (sort of)
qsort                                  rand
realloc                                srand
strtod                                 system
strtol
strtoul
wcstombs
wctomb

(from string.h)                        (from string.h)
memcmp                                 memcpy
memset                                 memmove
strcoll                                strcasecmp
strcspn                                strcat
strerror                               strcmp
strspn                                 strcpy
strtok                                 strlen
strxfrm                                strncat
                                       strncmp
				       strncpy

(from time.h)                          (from time.h)
ctime                                  asctime
gmtime                                 clock
localtime                              difftime
mktime
strftime
time

(from wctype.h)                        (from wctype.h)
iswalnum
iswalpha
iswcntrl
iswctype
iswdigit
iswgraph
iswlower
iswprint
iswpunct
iswspace
iswupper
iswxdigit
towctrans
towlower
towupper
wctrans
wctype

(from wchar.h)                          (from wchar.h)
btowc
fgetwc
fgetws
fputwc
fputws
fwide
fwprintf
fwscanf
getwc
getwchar
mbrlen
mbrtowc
mbsinit
mbsrtowcs
putwc
putwchar
swprintf
swscanf
ungetwc
vfwprintf
vswprintf
vwprintf
wcrtomb
wcscat
wcschr
wcscmp
wcscoll
wcscpy
wcscspn
wcsftime
wcslen
wcsncat
wcsncmp
wcsncpy
wcspbrk
wcsrchr
wcsrtombs
wcsspn
wcsstr
wcstod
wcstok
wcstol
wcstoul
wcsxfrm
wctob
wmemchr
wmemcmp
wmemcpy
wmemmove
wmemset
wprintf
wscanf
