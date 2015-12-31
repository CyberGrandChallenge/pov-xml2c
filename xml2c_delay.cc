/*
 * Id:             $Id: xml2c_delay.cc 3725 2015-02-10 23:48:55Z cseagle $
 * Last Updated:   $LastChangedDate: 2015-02-10 23:48:55 +0000 (Tue, 10 Feb 2015) $ 
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "utils.h"
#include "xml2c_delay.h"
#include "logging.h"

#include "reasons.h"

Xml2cDelay::Xml2cDelay(xmlNode *n) {
   unsigned int len;
   char *endptr;
   char *delayText = getNodeText(n, &len);
   char first = *delayText;
   if (len == 0) {
      log_fail("<%s> element contains no data at line %d\n", n->name, n->line);
      throw (int)PARSE_ERROR;
   }
   msec = strtoul(delayText, &endptr, 10);
   while (*endptr) {
      if (!isspace(*endptr)) {
         log_fail("Expected one unsigned integer in <%s> element at line %d\n", n->name, n->line);
         throw (int)PARSE_ERROR;
      }
      endptr++;
   } 
   xmlFree(delayText);
}
   
void Xml2cDelay::generate(FILE *outfile) {
   fprintf(outfile, "   //*** delay\n");
   fprintf(outfile, "   delay(%u);\n", msec);
}

