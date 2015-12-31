/*
 * Id:             $Id: xml2c_read.cc 6829 2015-06-12 05:37:32Z cseagle $
 * Last Updated:   $LastChangedDate: 2015-06-12 05:37:32 +0000 (Fri, 12 Jun 2015) $ 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <errno.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <pcre.h>
#include <new>

#include "xml2c_read.h"
#include "utils.h"
#include "logging.h"

#include "reasons.h"

using std::bad_alloc;

struct Slice {
   int32_t _begin;
   int32_t _end;
   bool _maxLen;
   
   Slice(uint32_t begin, uint32_t end, bool maxLen = true) : _begin(begin), _end(end), _maxLen(maxLen) {};
};

struct Regex {
   uint32_t _len;
   uint32_t group;
   uint32_t ngroups;
   char *expr;
   pcre *regex;
   
   Regex(xmlNode *n);
   ~Regex();
   vector<uint8_t> *match(const uint8_t *buf, uint32_t len, uint32_t *len0);
   vector<uint8_t> *match(vector<uint8_t> &buf, uint32_t *len0);
};

unsigned int Xml2cRead::readId = 0;

Regex::Regex(xmlNode *n) {   
   uint32_t len;
   expr = NULL;
   bool parseError = false;
   try {
      getUintAttribute(n, "group", 0, &group);
      expr = getNodeText(n, &len);
      regex = init_regex(expr);
      pcre_fullinfo(regex, NULL, PCRE_INFO_CAPTURECOUNT, &ngroups);
      if (group > ngroups) {
         //attempt to specify a matching group larger than max number of groups
         throw (int)INVALID_REGEX;
      }
      ngroups++;
   } catch (int ex) {
      if (expr != NULL) {
         xmlFree(expr);
      }
      throw (int)PARSE_ERROR;
   }
}

Regex::~Regex() {
   pcre_free(regex);
   xmlFree(expr);
}

vector<uint8_t> *Regex::match(const uint8_t *buf, uint32_t len, uint32_t *len0) {
   vector<uint8_t> *val = NULL;
   int *ovector = new int[ngroups * 3];
   int rc = pcre_exec(regex, NULL, (const char*)buf, len, 0, PCRE_ANCHORED, ovector, ngroups * 3);
   if (rc > 0) {
      int index  = group * 2;
      val = new vector<uint8_t>(buf + ovector[index], buf + ovector[index + 1]);
      *len0 = ovector[1];
   }
   delete [] ovector;
   return val;
}

vector<uint8_t> *Regex::match(vector<uint8_t> &buf, uint32_t *len0) {
   return match(buf.data(), buf.size(), len0);
}

class MatchPart {
public:
   virtual void generate(FILE *outfile, int id, int idx) = 0;
};

class VarMatch : public MatchPart {
   char *var;
public:
   VarMatch(xmlNode *n);
   ~VarMatch();
   void generate(FILE *outfile, int id, int idx);
};

VarMatch::VarMatch(xmlNode *n) {
   uint32_t len;
   var = getNodeText(n, &len);
}

VarMatch::~VarMatch() {
   xmlFree(var);
}

void VarMatch::generate(FILE *outfile, int id, int idx) {
   fprintf(outfile, "      //**** read match var %s\n", var);
   //it's a pov so we try to match but are permissive after failure
   fprintf(outfile, "      read_%05d_ptr += var_match(read_%05d + read_%05d_ptr, read_%05d_len - read_%05d_ptr, \"%s\");\n", id, id, id, id, id, var);
}

class DataMatch : public MatchPart {
   vector<uint8_t> *matchex;
public:
   DataMatch(xmlNode *n);
   ~DataMatch();
   void generate(FILE *outfile, int id, int idx);
};

DataMatch::DataMatch(xmlNode *n) {
   uint32_t len;
   matchex = NULL;
   int parseError = false;
   char *data = getNodeText(n, &len);
   char *format = (char*)xmlGetProp(n, (xmlChar*)"format");
   try {
      if (format != NULL && strcmp(format, "hex") == 0) {
         //hex data must be exact match
         matchex = parseHexBinary(data);
      }
      else if (format == NULL || strcmp(format, "asciic") == 0) {
         //hex data must be exact match
         matchex = unescapeAscii(data);
      }
      else {
         //should never get here
      }
   } catch (int ex) {
      switch (ex) {
         case INVALID_HEX:
            log_fail("Invalid hex data in <%s> element at line %d\n", n->name, n->line);
            break;
         case INVALID_REGEX:
            log_fail("Invalid regex in <%s> element at line %d\n", n->name, n->line);
            break;
      }
      parseError = true;
   }
   xmlFree(format);
   xmlFree(data);
   if (parseError) {
      throw (int)PARSE_ERROR;
   }
}

DataMatch::~DataMatch() {
   xmlFree(matchex);
}

//here we do a prefix match
void DataMatch::generate(FILE *outfile, int id, int idx) {
   fprintf(outfile, "      //**** read match data\n");
   fprintf(outfile, "      static unsigned char match_%05d_%05d[] = \n", id, idx);
   printAsHexString(outfile, matchex->data(), matchex->size());
   fprintf(outfile, "      read_%05d_ptr += data_match(read_%05d + read_%05d_ptr, read_%05d_len - read_%05d_ptr, match_%05d_%05d, %u);\n", id, id, id, id, id, id, idx, matchex->size());
}

class PcreMatch : public MatchPart {
   Regex *regex;
public:
   PcreMatch(xmlNode *n);
   ~PcreMatch();
   void generate(FILE *outfile, int id, int idx);
};

PcreMatch::PcreMatch(xmlNode *n) {
   uint32_t len;
   regex = new Regex(n);
   //matching is always against group 0
   //should add check to make sure that group is already 0 
   //and throw parse error otherwise
   regex->group = 0;   
}

PcreMatch::~PcreMatch() {
   delete regex;
}

void PcreMatch::generate(FILE *outfile, int id, int idx) {
   fprintf(outfile, "      /* read match pcre:\n%s\n*/\n", regex->expr);
   fprintf(outfile, "      static char read_%05d_%05d_regex[] = \n", id, idx);
   printAsHexString(outfile, (const unsigned char*)regex->expr, strlen(regex->expr));
   fprintf(outfile, "      static match_result read_%05d_%05d_match;\n", id, idx);
   fprintf(outfile, "      pcre *read_%05d_%05d_pcre = init_regex(read_%05d_%05d_regex);\n", id, idx, id, idx);
   fprintf(outfile, "      if (read_%05d_%05d_pcre != NULL) {\n", id, idx);
   fprintf(outfile, "         int rc = regex_match(read_%05d_%05d_pcre, %d, read_%05d + read_%05d_ptr, read_%05d_len - read_%05d_ptr, &read_%05d_%05d_match);\n", id, idx, regex->group, id, id, id, id, id, idx);
   fprintf(outfile, "         if (rc > 0) {\n");
   fprintf(outfile, "            read_%05d_ptr += read_%05d_%05d_match.match_end - read_%05d_%05d_match.match_start;\n", id, id, idx, id, idx);
   fprintf(outfile, "         }\n");
   fprintf(outfile, "         else {\n");
   fprintf(outfile, "            //this is a pov so what does this even mean?\n");
   fprintf(outfile, "            //why would we quit on failed match, just keep sending stuff.\n");
   fprintf(outfile, "         }\n");
   fprintf(outfile, "         pcre_free(read_%05d_%05d_pcre);\n", id, idx);
   fprintf(outfile, "      }\n");
   fprintf(outfile, "      else {\n");
   fprintf(outfile, "         //this is a pov so what does this even mean?\n");
   fprintf(outfile, "         //why would we quit on failed regex compile, just keep sending stuff.\n");
   fprintf(outfile, "      }\n");
}

Xml2cRead::Xml2cRead(xmlNode *r, bool echoEnable) {
   id = readId++;
   bool parseError = false;
   slice = NULL;
   varRegex = NULL;
   echo = ECHO_NO;
   invert = false;
   delim = NULL;
   var = NULL;
   lengthVar = NULL;
   readLen = 0;
   lengthIsVar = false;

   if (echoEnable) {
      char *echoAttr = (char*)xmlGetProp(r, (xmlChar*)"echo");
      if (echoAttr != NULL) {
         if (strcmp(echoAttr, "no") == 0) {
            echo = ECHO_NO;
         }
         else if (strcmp(echoAttr, "yes") == 0) {
            echo = ECHO_YES;
         }
         else {
            echo = ECHO_ASCII;
         }
      }
      xmlFree(echoAttr);
   }
   xmlNode *delimNode = findChild(r, "delim");
   if (delimNode != NULL) {
      //??? What if any restrictions to place on delimiters? content? length?
      uint32_t delimLen;
      char *delimText = getNodeText(delimNode, &delimLen);
      char *format = (char*)xmlGetProp(delimNode, (xmlChar*)"format");
      try {
         if (format != NULL && strcmp(format, "hex") == 0) {
            delim = parseHexBinary(delimText);
         }
         else {
            delim = unescapeAscii(delimText);
         }
      } catch (int ex) {
         parseError = true;
         log_fail("Invalid hex data in <%s> element at line %d\n", delimNode->name, delimNode->line);
      }
      xmlFree(delimText);
      xmlFree(format);
   }

   if (delimNode == NULL) {
      //must have a length, can't have both
      try {
         xmlNode *lengthNode = findChild(r, "length");

         char *isvar = (char*)xmlGetProp(lengthNode, (xmlChar*)"isvar");
         if (isvar != NULL) {
            lengthIsVar = strcmp(isvar, "true") == 0;
         }
         xmlFree(isvar);
         if (!lengthIsVar) {
            readLen = getUintChild(r, "length", 0);
         }
         else {
            uint32_t lengthVarLen;
            lengthVar = getNodeText(lengthNode, &lengthVarLen);
         }
      } catch (int ex) {
         parseError = true;
      }
   }

   xmlNode *assignNode = findChild(r, "assign");
   if (assignNode != NULL) {
      try {
         uint32_t varLen;
         xmlNode *varNode = findChild(assignNode, "var");
         var = getNodeText(varNode, &varLen);
         xmlNode *sliceNode = findChild(assignNode, "slice");
         if (sliceNode != NULL) {
            int begin, end;
            getIntAttribute(sliceNode, "begin", 0, &begin);
            bool useMax = !getIntAttribute(sliceNode, "end", 0, &end);
            slice = new Slice(begin, end, useMax);
         }
         else {
            //must be a pcre, must have one, can't have both
            xmlNode *pcreNode = findChild(assignNode, "pcre");
            varRegex = new Regex(pcreNode);
         }
      } catch (int ex) {
         parseError = true;
         log_fail("Error parsing <%s> element at line %d\n", assignNode->name, assignNode->line);
      }
   }
   
   xmlNode *match = findChild(r, "match");
   if (match != NULL) {
      char *invertAttr = (char*)xmlGetProp(match, (xmlChar*)"invert");
      if (invertAttr != NULL) {
         invert = strcmp(invertAttr, "true") == 0;
      }
      xmlFree(invertAttr);

      try {
         for (xmlNode *child = match->children; child != NULL; child = child->next) {
            char *type = (char*)child->name;
            if (strcmp(type, "data") == 0) {
               matchParts.push_back(new DataMatch(child));
            }
            else if (strcmp(type, "var") == 0) {
               matchParts.push_back(new VarMatch(child));
            }
            else if (strcmp(type, "pcre") == 0) {
               matchParts.push_back(new PcreMatch(child));
            }
         }
      } catch (int ex) {
         parseError = true;
         log_fail("Error parsing <%s> element at line %d\n", match->name, match->line);
      }
   }

   try {
      timeout_val = getUintChild(r, "timeout", 0);
      timeout.tv_sec = timeout_val / 1000;
      timeout.tv_usec = (timeout_val % 1000) * 1000;
   } catch (int ex) {
      parseError = true;
   }
   if (parseError) {
      throw (int)PARSE_ERROR;
   }
}

Xml2cRead::~Xml2cRead() {
   delete slice; 
   delete varRegex;

   for (vector<MatchPart*>::iterator i = matchParts.begin(); i != matchParts.end(); i++) {
      delete *i;
   }

   if (delim != NULL) {
      delete delim;
   }
   if (var != NULL) {
      xmlFree(var);
   }
   if (lengthVar != NULL) {
      xmlFree(lengthVar);
   }
}

void Xml2cRead::doRead(FILE *outfile) {
   if (delim == NULL) {  //then readLen or lengthVar must be set
      //can't really account for timeouts in PoVs
      fprintf(outfile, "      //**** length read\n");
      if (lengthVar != NULL) {
         fprintf(outfile, "      size_t read_%05d_len_len;\n", id);
         fprintf(outfile, "      char *read_%05d_len_var = (char*)getenv(\"%s\", &read_%05d_len_len);\n", id, lengthVar, id);
         fprintf(outfile, "      read_%05d_len = *(unsigned int*)read_%05d_len_var;\n", id, id);
         fprintf(outfile, "      free(read_%05d_len_var);\n", id);
      }
      else {
         fprintf(outfile, "      read_%05d_len = %u;\n", id, readLen);
      }
      fprintf(outfile, "      read_%05d = (unsigned char*)malloc(read_%05d_len);\n", id, id);
      fprintf(outfile, "      int read_%05d_res = length_read(0, read_%05d, read_%05d_len);\n", id, id, id);
      fprintf(outfile, "      if (read_%05d_res) {} //silence unused variable warning\n", id);
      //do we need code to test for short read? What would we do in any case?
   }
   else {
      //can't really account for timeouts in PoVs
      fprintf(outfile, "      //**** delimited read\n");
      fprintf(outfile, "      static unsigned char read_%05d_delim[] = \n", id);
      printAsHexString(outfile, delim->data(), delim->size());
      fprintf(outfile, "      read_%05d = NULL;\n", id);
      fprintf(outfile, "      read_%05d_len = 0;\n", id);
      fprintf(outfile, "      int read_%05d_res = delimited_read(0, &read_%05d, &read_%05d_len, read_%05d_delim, %u);\n", id, id, id, id, delim->size());
      fprintf(outfile, "      if (read_%05d_res) {} //silence unused variable warning\n", id);
      //do we need code to test for failed read? What would we do in any case?
   }
}

void Xml2cRead::generate(FILE *outfile) {
   fprintf(outfile, "   do {\n");
   fprintf(outfile, "      unsigned char *read_%05d;\n", id);
   fprintf(outfile, "      unsigned int read_%05d_len;\n", id);
   fprintf(outfile, "      unsigned int read_%05d_ptr = 0;\n", id);

   doRead(outfile);

   if (matchParts.size() != 0) {
      //do some matching
      uint32_t idx = 0;  //for serializing match components
      for (vector<MatchPart*>::iterator i = matchParts.begin(); i != matchParts.end(); i++) {
         (*i)->generate(outfile, id, idx);
         idx++;
      }
      //need code to deal with invert
   }

   if (var != NULL) {
      if (slice != NULL) {
         fprintf(outfile, "      //**** read assign to var \"%s\" from slice\n", var);
         fprintf(outfile, "      assign_from_slice(\"%s\", read_%05d, read_%05d_len - read_%05d_ptr, %d, %d, %d);\n", var, id, id, id, slice->_begin, slice->_end, slice->_maxLen);
      }
      else {  //must be pcre
         fprintf(outfile, "      //**** read assign to var \"%s\" from pcre: %s\n", var, varRegex->expr);
         fprintf(outfile, "      static char read_%05d_regex[] = \n", id);
         printAsHexString(outfile, (const unsigned char*)varRegex->expr, strlen(varRegex->expr));
         fprintf(outfile, "      assign_from_pcre(\"%s\", read_%05d, read_%05d_len - read_%05d_ptr, read_%05d_regex, %d);\n", var, id, id, id, id, varRegex->group);
      }
   }
   fprintf(outfile, "      free(read_%05d);\n", id);
   fprintf(outfile, "      if (read_%05d_ptr) {}  //silence unused variable warning if any\n", id);
   fprintf(outfile, "   } while (0);\n");
}
