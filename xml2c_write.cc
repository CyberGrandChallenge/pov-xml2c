/*
 * Id:             $Id: xml2c_write.cc 6829 2015-06-12 05:37:32Z cseagle $
 * Last Updated:   $LastChangedDate: 2015-06-12 05:37:32 +0000 (Fri, 12 Jun 2015) $ 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <sys/socket.h>
#include <errno.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <unistd.h>

#include "xml2c_write.h"
#include "utils.h"
#include "logging.h"

#include "reasons.h"

unsigned int Xml2cWrite::writeId = 0;

Xml2cWrite::Xml2cWrite(xmlNode *w, bool echoEnable) {
   id = writeId++;
   bool parseError = false;
   echo = ECHO_NO;

   if (echoEnable) {
      char *echoAttr = (char*)xmlGetProp(w, (xmlChar*)"echo");
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
   
   vector<uint8_t> *el = NULL;
   for (xmlNode *d = w->children; d; d = d->next) {
      if (strcmp((char*)d->name, "data") == 0) {
         char *format = (char*)xmlGetProp(d, (xmlChar*)"format");
         unsigned int tlen;
         char *dataString = getNodeText(d, &tlen);
         if (tlen > 0) {
            if (el == NULL) {
               el = new vector<uint8_t>;
               data.push_back(el);
            }
            try {
               if (format != NULL && strcmp(format, "hex") == 0) {
                  unsigned int hlen;
                  vector<uint8_t> *hex = parseHexBinary(dataString);
                  if (hex->size() > 0) {
                     el->insert(el->end(), hex->begin(), hex->end());
                     delete hex;
                  }
                  else {
                     //this is a problem that should have been reported in utils.cc
                  }
               }
               else { //format defaults to ascii
                  vector<uint8_t> *asc = unescapeAscii((char*)dataString);
                  el->insert(el->end(), asc->begin(), asc->end());
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
         el = NULL;
         uint32_t tlen;
         char *varName = getNodeText(d, &tlen);
         vars.push_back(varName);
         data.push_back(NULL); 
         xmlFree(varName);
      }
   }

   if (parseError) {
      throw (int)PARSE_ERROR;
   }
}

Xml2cWrite::~Xml2cWrite() {
   for (vector< vector<uint8_t>* >::iterator i = data.begin(); i != data.end(); i++) {
      delete *i;
   }
}

void Xml2cWrite::generate(FILE *outfile) {
   fprintf(outfile, "   do {\n");
   fprintf(outfile, "      //*** writing data\n");
   //loop to add static declarations for all the static bits
   unsigned int idx = 0;
   for (vector< vector<uint8_t>* >::iterator i = data.begin(); i != data.end(); i++) {
      if (*i != NULL) {
         fprintf(outfile, "      static unsigned char write_%05d_%05d[] = \n", id, idx);
         printAsHexString(outfile, (*i)->data(), (*i)->size());
         fprintf(outfile, "      static unsigned int write_%05d_%05d_len = %u;\n", id, idx, (*i)->size());
      }
      idx++;
   }

   fprintf(outfile, "      unsigned char *write_%05d = NULL;\n", id);
   fprintf(outfile, "      unsigned int write_%05d_len = 0;\n", id);

   //loop again to paste it all together

   idx = 0;
   vector<string>::iterator s = vars.begin();
   for (vector< vector<uint8_t>* >::iterator i = data.begin(); i != data.end(); i++) {
      if (*i == NULL) {
         //expand a variable
         fprintf(outfile, "      write_%05d = append_var(\"%s\", write_%05d, &write_%05d_len);\n", id, s->c_str(), id, id);
         s++;
      }
      else {
         fprintf(outfile, "      write_%05d = append_buf(write_%05d, &write_%05d_len, write_%05d_%05d, write_%05d_%05d_len);\n", id, id, id, id, idx, id, idx);
      }
      idx++;
   }
   fprintf(outfile, "      if (write_%05d_len > 0) {\n", id);
   fprintf(outfile, "         transmit_all(1, write_%05d, write_%05d_len);\n", id, id);
   fprintf(outfile, "      }\n");
   fprintf(outfile, "      free(write_%05d);\n", id);

   fprintf(outfile, "   } while (0);\n");

}

