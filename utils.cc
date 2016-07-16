/*
 * Id:             $Id: utils.cc 5410 2015-04-15 04:22:29Z cseagle $
 * Last Updated:   $LastChangedDate: 2015-04-15 04:22:29 +0000 (Wed, 15 Apr 2015) $ 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <unistd.h>
#include <pcre.h>
#include <stdarg.h>

#include "utils.h"
#include "logging.h"

pcre *init_regex(char *pattern) {
   pcre *regex;
   const char *error;
   int erroffset;
#ifdef DEBUG
   log("compiling pattern \"%s\"\n", pattern);
#endif
   regex = pcre_compile(pattern, PCRE_DOTALL, &error, &erroffset, NULL);
   if (regex == NULL) {
      log_fail("regex compilation for: %s\n# failed at offset %d: %s\n", pattern, erroffset, error);
      throw (int)INVALID_REGEX;
   }
   return regex;
}

static int hexValue(char ch) {
   if (isxdigit(ch)) {
      if (isdigit(ch)) {
         return ch - '0';
      }
      else {
         return toupper(ch) - 'A' + 10;
      }
   }
   throw (int)INVALID_HEX;
}

void printAsHexString(FILE *outfile, const unsigned char *bin, uint32_t len) {
   uint32_t lines = 0;
   uint32_t cols = 0;
   for (uint32_t i = 0; i < len; i++) {
      if ((i % 16) == 0) {
         if (i > 0) {
            //wrap up the current line
            fprintf(outfile, "\"\n");      
         }
         //start a new line
         fprintf(outfile, "         \"");      
      }
      fprintf(outfile, "\\x%02x", bin[i]);
   }
   //wrap up the last line
   fprintf(outfile, "\";\n");   
}

uint8_t *parseHexBinary(const char *hex, uint32_t *len) {
   uint32_t hlen = strlen(hex);
   uint8_t *res = NULL;
   uint32_t i = 0;
   *len = 0;
   res = new uint8_t[hlen / 2];
   while (hex[i]) {
      if (isspace(hex[i])) {
         i++;
      }
      else {
         try {
            res[*len] = hexValue(hex[i]) * 16 + hexValue(hex[i + 1]);
         } catch (int ex) {
            delete [] res;
            throw ex;
         }
         i += 2;
         (*len)++;
      }      
   }
   return res;
}

vector<uint8_t> *parseHexBinary(const char *hex) {
   vector<uint8_t> *res = new vector<uint8_t>;
   uint32_t i = 0;
   for (uint32_t i = 0; hex[i]; i++) {
      if (!isspace(hex[i])) {
         try {
            res->push_back(hexValue(hex[i]) * 16 + hexValue(hex[i + 1]));
         } catch (int ex) {
            delete res;
            throw ex;
         }
         i++;
      }      
   }
   return res;
}

uint8_t *unescapeAscii(const char *asc, uint32_t *len) {
   uint32_t alen = 0;
   uint32_t byteVal = 0;
   uint8_t *res = new uint8_t[*len];
   int state = 0;   //1 == escape, 2 == hex1, 3 == hex2
   for (uint32_t i = 0; i < *len; i++) {
      char ch = asc[i];
      switch (state) {
         case 0:
            if (ch == '\\') {
               state = 1;
            }
            else {
               res[alen++] = ch;
            }
            break;
         case 1:
            switch (ch) {
               case 'n':
                  res[alen++] = '\n';
                  state = 0;
                  break;
               case 'r':
                  res[alen++] = '\r';
                  state = 0;
                  break;
               case 't':
                  res[alen++] = '\t';
                  state = 0;
                  break;
               case 'x': case 'X':
                  state = 2;
                  break;
               default:
                  res[alen++] = ch;
                  state = 0;
                  break;
            }
            break;
         case 2:
            try {
               byteVal = hexValue(ch) * 16;
            } catch (int ex) {
               delete [] res;
               throw ex;
//               log_fail("Error unescaping ascii string \"%s\" at position %d\n", asc, i);
//               log("Hex digit expected, found: '%c'\n", ch);
            }
            state = 3;
            break;
         case 3:
            try {
               byteVal += hexValue(ch);
            } catch (int ex) {
               delete [] res;
               throw ex;
//               log_fail("Error unescaping ascii string \"%s\" at position %d\n", asc, i);
//               log("Hex digit expected, found: '%c'\n", ch);
            }
            res[alen++] = byteVal;
            state = 0;
            break;
      }
   }
   *len = alen;
   return res;
}

vector<uint8_t> *unescapeAscii(const char *asc) {
   uint32_t byteVal = 0;
   vector<uint8_t> *res = new vector<uint8_t>;
   int state = 0;   //1 == escape, 2 == hex1, 3 == hex2
   while (*asc) {
      char ch = *asc++;
      switch (state) {
         case 0:
            if (ch == '\\') {
               state = 1;
            }
            else {
               res->push_back(ch);
            }
            break;
         case 1:
            switch (ch) {
               case 'n':
                  res->push_back('\n');
                  state = 0;
                  break;
               case 'r':
                  res->push_back('\r');
                  state = 0;
                  break;
               case 't':
                  res->push_back('\t');
                  state = 0;
                  break;
               case 'x': case 'X':
                  state = 2;
                  break;
               default:
                  res->push_back(ch);
                  state = 0;
                  break;
            }
            break;
         case 2:
            try {
               byteVal = hexValue(ch) * 16;
            } catch (int ex) {
               delete res;
               throw ex;
//               log_fail("Error unescaping ascii string \"%s\" at position %d\n", asc, i);
//               log("Hex digit expected, found: '%c'\n", ch);
            }
            state = 3;
            break;
         case 3:
            try {
               byteVal += hexValue(ch);
            } catch (int ex) {
               delete res;
               throw ex;
//               log_fail("Error unescaping ascii string \"%s\" at position %d\n", asc, i);
//               log("Hex digit expected, found: '%c'\n", ch);
            }
            res->push_back(byteVal);
            state = 0;
            break;
      }
   }
   return res;
}

xmlNode *findChild(xmlNode *parent, const char *childTag) {
   if (parent != NULL) {
      for (xmlNode *nl = parent->children; nl; nl = nl->next) {
         if (strcmp((char*)nl->name, childTag) == 0) {
            return nl;
         }
      }
   }
   return NULL;
}

char *getNodeText(xmlNode *n, uint32_t *len) {
   xmlBufferPtr buf = xmlBufferCreate();
   xmlNodeBufGetContent(buf, n);
   *len = xmlBufferLength(buf);
   xmlChar *data = xmlBufferDetach(buf);
   xmlBufferFree(buf);
   return (char*)data;
}

char *getAttributeText(xmlNode *n, const char *name) {
   if (n != NULL) {
      xmlChar *attr = xmlGetProp(n, (xmlChar*)name);
      return (char*)attr;
   }
   return NULL;
}

bool getIntAttribute(xmlNode *n, const char *attr, int defValue, int *value) {
   *value = defValue;
   if (n != NULL) {
      char *attrText = (char*)xmlGetProp(n, (xmlChar*)attr);
      if (attrText != NULL) {
         char *endptr;
         int res = strtol(attrText, &endptr, 10);
         if (endptr == attrText) {
            throw (int)INVALID_INT; //no data was present
         }
         while (*endptr) {
            if (!isspace(*endptr)) {
               log_fail("Expected one integer in <%s> element %s attribute at line %d\n", n->name, attr, n->line);
               throw (int)INVALID_INT;
            }
            endptr++;
         }
         *value = res;
         xmlFree(attrText);
         return true;
      }
   }
   return false;
}

bool getUintAttribute(xmlNode *n, const char *attr, uint32_t defValue, uint32_t *value) {
   *value = defValue;
   if (n != NULL) {
      char *attrText = (char*)xmlGetProp(n, (xmlChar*)attr);
      if (attrText != NULL) {
         char *endptr;
         uint32_t res = strtoul(attrText, &endptr, 10);
         if (endptr == attrText) {
            throw (int)INVALID_INT; //no data was present
         }
         while (*endptr) {
            if (!isspace(*endptr)) {
               log_fail("Expected one integer in <%s> element %s attribute at line %d\n", n->name, attr, n->line);
               throw (int)INVALID_INT;
            }
            endptr++;
         }
         *value = res;
         xmlFree(attrText);
         return true;
      }
   }
   return false;
}

int32_t getIntChild(xmlNode *n, const char *tag, int32_t defaultVal) {
   xmlNode *c = findChild(n, tag);
   if (c == NULL) {
      return defaultVal;
   }
   uint32_t len;
   char *endptr;
   char *data = (char*)getNodeText(c, &len);
   if (len == 0) {
      log_fail("<%s> element contains no data at line %d\n", c->name, c->line);
      throw (int)INVALID_INT;
   }
   int32_t res = strtol(data, &endptr, 10);

   while (*endptr) {
      if (!isspace(*endptr)) {
         log_fail("Expected one integer in <%s> element at line %d\n", c->name, c->line);
         throw (int)INVALID_INT;
      }
      endptr++;
   }
   xmlFree(data);
   return res;
}

uint32_t getUintChild(xmlNode *n, const char *tag, uint32_t defaultVal) {
   xmlNode *c = findChild(n, tag);
   if (c == NULL) {
      return defaultVal;
   }
   uint32_t len;
   char *endptr;
   char *data = (char*)getNodeText(c, &len);
   if (len == 0) {
      log_fail("<%s> element contains no data at line %d\n", c->name, c->line);
      throw (int)INVALID_UINT;
   }
   uint32_t res = strtoul(data, &endptr, 10);

   while (*endptr) {
      if (!isspace(*endptr)) {
         log_fail("Expected one unsigned integer in <%s> element at line %d\n", c->name, c->line);
         throw (int)INVALID_UINT;
      }
      endptr++;
   }
   xmlFree(data);
   return res;
}

char *getStringChild(xmlNode *n, const char *tag, const char *defaultVal, uint32_t *len) {
   xmlNode *c = findChild(n, tag);
   if (c == NULL) {
      return (char*)defaultVal;
   }
   return (char*)getNodeText(c, len);
}

vector<uint8_t> *vectorFromBuffer(const uint8_t *buf, uint32_t size) {
   vector<uint8_t> *res = new vector<uint8_t>;
   res->reserve(size);
   for (uint32_t i = 0; i < size; i++) {
      res->push_back(buf[i]);
   }
   return res;
}

void printHex(const char *prefix, const uint8_t *data, uint32_t len) {
   char *hex = new char[2 * len + 1];
   for (uint32_t i = 0; i < len; i++) {
      snprintf(hex + 2 * i, 3, "%02x", data[i]);
   }
   log("%s: %s\n", prefix, hex);
   delete [] hex;
}

void printHex(const char *prefix, const vector<uint8_t> *data) {
   printHex(prefix, data->data(), data->size());
}
