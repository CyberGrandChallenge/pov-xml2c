/*
 * Id:             $Id: utils.h 5410 2015-04-15 04:22:29Z cseagle $
 * Last Updated:   $LastChangedDate: 2015-04-15 04:22:29 +0000 (Wed, 15 Apr 2015) $ 
 */

#ifndef __REPLAY_UTILS_H
#define __REPLAY_UTILS_H

#include <stdint.h>
#include <stdio.h>
#include <libxml/tree.h>
#include <pcre.h>
#include <vector>

using std::vector;

enum {
   INVALID_UINT,
   INVALID_INT,
   INVALID_HEX,
   INVALID_REGEX,
   PARSE_ERROR,
   PARSE_TIMEOUT,
   PLAY_TIMEOUT
};

pcre *init_regex(char *pattern);

void printAsHexString(FILE *outfile, const unsigned char *bin, uint32_t len);
uint8_t *parseHexBinary(const char *hex, uint32_t *len);
vector<uint8_t> *parseHexBinary(const char *hex);
uint8_t *unescapeAscii(const char *asc, uint32_t *len);
vector<uint8_t> *unescapeAscii(const char *asc);

xmlNode *findChild(xmlNode *parent, const char *childTag);
char *getNodeText(xmlNode *n, uint32_t *len);
char *getAttributeText(xmlNode *n, const char *name);
bool getIntAttribute(xmlNode *n, const char *attr, int defValue, int *value);
bool getUintAttribute(xmlNode *n, const char *attr, uint32_t defValue, uint32_t *value);
int32_t getIntChild(xmlNode *n, const char *tag, int32_t defaultVal);
uint32_t getUintChild(xmlNode *n, const char *tag, uint32_t defaultVal);
char *getStringChild(xmlNode *n, const char *tag, const char *defaultVal, uint32_t *len);
vector<uint8_t> *vectorFromBuffer(const uint8_t *buf, uint32_t size);

void printHex(const char *prefix, const uint8_t *data, uint32_t len);
void printHex(const char *prefix, const vector<uint8_t> *data);

#endif

