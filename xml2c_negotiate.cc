/*
 * Id:             $Id: xml2c_negotiate.cc 4753 2015-03-26 06:36:07Z cseagle $
 * Last Updated:   $LastChangedDate: 2015-03-26 06:36:07 +0000 (Thu, 26 Mar 2015) $ 
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "utils.h"
#include "xml2c_negotiate.h"
#include "logging.h"

#include "reasons.h"

Xml2cNegotiate::Xml2cNegotiate(xmlNode *n) {
   xmlNode *typeNode = findChild(n, "type1");
   if (typeNode == NULL) {
      typeNode = findChild(n, "type2");
      if (typeNode == NULL) {
         log_fail("Missing type1 or type2 in <%s> element at line %d\n", n->name, n->line);
         throw (int)PARSE_ERROR;
      }
      parseType2(typeNode);
   }
   else {
      parseType1(typeNode);
   }
}

void Xml2cNegotiate::parseType1(xmlNode *type1) {
   povType = 1;
   char *endptr;
   unsigned int len;
   xmlNode *ipMaskNode = findChild(type1, "ipmask");
   xmlNode *regMaskNode = findChild(type1, "regmask");
   xmlNode *regNumNode = findChild(type1, "regnum");
   char *ipmaskStr = getNodeText(ipMaskNode, &len);
   if (len == 0) {
      log_fail("<%s> element contains no data at line %d\n", ipMaskNode->name, ipMaskNode->line);
      throw (int)PARSE_ERROR;
   }
   ipmask = strtoul(ipmaskStr, &endptr, 0);
   while (*endptr) {
      if (!isspace(*endptr)) {
         log_fail("Expected one unsigned integer in <%s> element at line %d\n", ipMaskNode->name, ipMaskNode->line);
         throw (int)PARSE_ERROR;
      }
      endptr++;
   } 

   char *regmaskStr = getNodeText(regMaskNode, &len);
   if (len == 0) {
      log_fail("<%s> element contains no data at line %d\n", regMaskNode->name, regMaskNode->line);
      throw (int)PARSE_ERROR;
   }
   regmask = strtoul(regmaskStr, &endptr, 0);
   while (*endptr) {
      if (!isspace(*endptr)) {
         log_fail("Expected one unsigned integer in <%s> element at line %d\n", regMaskNode->name, regMaskNode->line);
         throw (int)PARSE_ERROR;
      }
      endptr++;
   }

   char *regnumStr = getNodeText(regNumNode, &len);
   if (len == 0) {
      log_fail("<%s> element contains no data at line %d\n", regNumNode->name, regNumNode->line);
      throw (int)PARSE_ERROR;
   }
   regnum = strtoul(regnumStr, &endptr, 10);
   while (*endptr) {
      if (!isspace(*endptr)) {
         log_fail("Expected one unsigned integer in <%s> element at line %d\n", regNumNode->name, regNumNode->line);
         throw (int)PARSE_ERROR;
      }
      endptr++;
   }
   if (regnum > 7) {
      log_fail("Expected integer in the range 0..7 (found %d) in <%s> element at line %d\n", regnum, regNumNode->name, regNumNode->line);
      throw (int)PARSE_ERROR;
   }

   xmlFree(ipmaskStr);
   xmlFree(regmaskStr);
   xmlFree(regnumStr);
}

void Xml2cNegotiate::parseType2(xmlNode *type2) {
   povType = 2;
}

void Xml2cNegotiate::generate(FILE *outfile) {
   switch (povType) {
      case 1:
         fprintf(outfile, "   negotiate_type1(0x%x, 0x%x, %u);\n", ipmask, regmask, regnum);
         break;
      case 2:
         fprintf(outfile, "   negotiate_type2();\n");
         break;
      default:
         log_fail("Invalid pov type (%d) encountered while generating source\n", povType);
         throw (int)PARSE_ERROR;
   }
}

PovSubmit::PovSubmit(xmlNode *n) {
   unsigned int varLen;
   xmlNode *varNode = findChild(n, "var");
   var = getNodeText(varNode, &varLen);
   povType = 0;
}

PovSubmit::~PovSubmit() {
   if (var != NULL) {
      xmlFree(var);
   }
}

void PovSubmit::generate(FILE *outfile) {
   if (povType == 2) {
      fprintf(outfile, "   //*** submitting type 2 POV results\n");
      if (var != NULL) {
         fprintf(outfile, "   submit_type2(\"%s\");\n", var);
      }
      else {
         fprintf(outfile, "   submit_type2(NULL);\n");
      }
   }
}
