/**
 *
MIT License

Copyright (c) 2018 Berke Emrecan ARSLAN

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

 */



#ifndef BSERVER_INIPARSE_H
#define BSERVER_INIPARSE_H

#define INI_KEY_LENGTH 16
#define INI_VAL_LENGTH 64
#define INI_COMMENT_PREFIX ";;"

typedef struct __inientry inientry;

struct __inientry {
    char *key; // DOH! 16 CHARACTERS LONG EXTENSIONS :D
    char *value; // well, maybe ?
    inientry *next; // linked lists
};

void ini_parse_file(char *, inientry **); // filename, head of list

void _ini_parse_line(char *, inientry *); // line: in, inientry: out

void ini_destroy(inientry *); // head of list

void ini_get(inientry *, char *, char **); // head in, ext in, value out

void remove_blank(char *); // str

#endif //BSERVER_INIPARSE_H
