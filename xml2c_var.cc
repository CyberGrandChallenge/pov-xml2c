/*
 * Id:             $Id: xml2c_var.cc 6829 2015-06-12 05:37:32Z cseagle $
 * Last Updated:   $LastChangedDate: 2015-06-12 05:37:32 +0000 (Fri, 12 Jun 2015) $ 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <unistd.h>
#include <limits.h>

#include "xml2c_var.h"
#include "utils.h"
#include "logging.h"

#include "reasons.h"

class Xml2cValue {
private:
   static uint32_t serial;
protected:
   uint32_t id;
   void printName(FILE *outfile);
public:
   Xml2cValue();
   virtual ~Xml2cValue() {};
   virtual void doDecls(FILE *outfile) = 0;
   virtual void generate(FILE *outfile, int varno) = 0;
};

uint32_t Xml2cValue::serial = 0;

Xml2cValue::Xml2cValue() {
   id = serial++;
}

void Xml2cValue::printName(FILE *outfile) {
   fprintf(outfile, "dvar_%08d", id);
}

class Xml2cValueData : public Xml2cValue {
private:
   vector<uint8_t> data;

public:
   Xml2cValueData(const vector<uint8_t> &);
   void doDecls(FILE *outfile);
   void generate(FILE *outfile, int varno);
};

class Xml2cValueVar : public Xml2cValue {
private:
   string name;
public:
   Xml2cValueVar(const char *_name);
   void doDecls(FILE *outfile) {};
   void generate(FILE *outfile, int varno);
};

class Xml2cValueSubstr : public Xml2cValue {
private:
   string name;
   int32_t begin;
   int32_t end;
public:
   Xml2cValueSubstr(const char *_name, int32_t _begin, int32_t _end);
   void doDecls(FILE *outfile) {};
   void generate(FILE *outfile, int varno);
};

Xml2cValueData::Xml2cValueData(const vector<uint8_t> &_data) {
   data.insert(data.end(), _data.begin(), _data.end());
}

void Xml2cValueData::doDecls(FILE *outfile) {
   fprintf(outfile, "      static unsigned char ");
   printName(outfile);
   fprintf(outfile, "[] = \n");
   printAsHexString(outfile, data.data(), data.size());
   fprintf(outfile, "      static unsigned int ");
   printName(outfile);
   fprintf(outfile, "_len = %u;\n", data.size());
}

void Xml2cValueData::generate(FILE *outfile, int varno) {
   fprintf(outfile, "      var_%05d = append_buf(var_%05d, &var_%05d_len, ", varno, varno, varno);
   printName(outfile);
   fprintf(outfile, ", ");
   printName(outfile);
   fprintf(outfile, "_len);\n");
}

Xml2cValueVar::Xml2cValueVar(const char *_name) {
   name = _name;
}

void Xml2cValueVar::generate(FILE *outfile, int varno) {
   fprintf(outfile, "      var_%05d = append_var(\"%s\", var_%05d, &var_%05d_len);\n", varno, name.c_str(), varno, varno);
}

Xml2cValueSubstr::Xml2cValueSubstr(const char *_name, int32_t _begin, int32_t _end) {
   name = _name;
   begin = _begin;
   end = _end;
}

void Xml2cValueSubstr::generate(FILE *outfile, int varno) {
   fprintf(outfile, "      var_%05d = append_slice(\"%s\", %d, %d, var_%05d, &var_%05d_len);\n", varno, name.c_str(), begin, end, varno, varno);
}

unsigned int Xml2cVar::varId = 0;

Xml2cVar::Xml2cVar(xmlNode *r) {
   id = varId++;
   bool parseError = false;
   uint32_t nameLen;
   xmlNode *nameNode = findChild(r, "var");
   char *cname = getNodeText(nameNode, &nameLen);
   name = cname;
   
   xmlNode *valueNode = findChild(r, "value");

   for (xmlNode *d = valueNode->children; d; d = d->next) {
      if (strcmp((char*)d->name, "data") == 0) {
         char *format = (char*)xmlGetProp(d, (xmlChar*)"format");
         unsigned int tlen;
         char *dataString = getNodeText(d, &tlen);
         if (tlen > 0) {
            try {
               if (format != NULL && strcmp(format, "hex") == 0) {
                  unsigned int hlen;
                  vector<uint8_t> *hex = parseHexBinary(dataString);
                  if (hex->size() != 0) {
                     Xml2cValueData *d = new Xml2cValueData(*hex);
                     values.push_back(d);
                     delete hex;
                  }
                  else {
                     //this is a problem that should have been reported in utils.cc
                  }
               }
               else { //default format is "ascii"
                  vector<uint8_t> *asc = unescapeAscii((char*)dataString);
                  Xml2cValueData *d = new Xml2cValueData(*asc);
                  values.push_back(d);
                  delete asc;
               }
            } catch (int ex) {
               log_fail("Invalid hex data in <%s> element at line %d\n", d->name, d->line);
               parseError = true;
            }
         }
         xmlFree(format);
         xmlFree(dataString);
      }
      else if (strcmp((char*)d->name, "var") == 0) {
         uint32_t tlen;
         char *varName = getNodeText(d, &tlen);
         Xml2cValueVar *v = new Xml2cValueVar(varName);
         values.push_back(v);
         xmlFree(varName);
      }
      else if (strcmp((char*)d->name, "substr") == 0) {
         uint32_t tlen;

         char *varName = getStringChild(d, "var", NULL, &tlen);
         int32_t begin = getIntChild(d, "begin", 0);
         int32_t end = getIntChild(d, "end", INT_MAX);

         Xml2cValueSubstr *s = new Xml2cValueSubstr(varName, begin, end);
         values.push_back(s);
         xmlFree(varName);
      }
   }
   xmlFree(cname);
   if (parseError) {
      throw (int)PARSE_ERROR;
   }
}

Xml2cVar::~Xml2cVar() {
   for (vector<Xml2cValue*>::iterator i = values.begin(); i != values.end(); i++) {
      delete *i;
   }
}   

void Xml2cVar::generate(FILE *outfile) {
   fprintf(outfile, "   do {\n");
   fprintf(outfile, "      //*** variable declaration for %s\n", name.c_str());
   //loop to add static declarations for all the static bits
   for (vector< Xml2cValue* >::iterator i = values.begin(); i != values.end(); i++) {
      (*i)->doDecls(outfile);
   }

   fprintf(outfile, "      unsigned char *var_%05d = NULL;\n", id);
   fprintf(outfile, "      unsigned int var_%05d_len = 0;\n", id);

   //loop again to paste it all together

   for (vector< Xml2cValue* >::iterator i = values.begin(); i != values.end(); i++) {
      (*i)->generate(outfile, id);
   }
   fprintf(outfile, "      putenv(\"%s\", var_%05d, var_%05d_len);\n", name.c_str(), id, id);
   fprintf(outfile, "      free(var_%05d);\n", id);

   fprintf(outfile, "   } while (0);\n");
}
