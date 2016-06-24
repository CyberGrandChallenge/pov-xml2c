/*
 * Id:             $Id: xml2c_negotiate.h 4125 2015-02-26 13:04:30Z cseagle $
 * Last Updated:   $LastChangedDate: 2015-02-26 13:04:30 +0000 (Thu, 26 Feb 2015) $ 
 */

#ifndef __XML2C_NEGOTIATE_H
#define __XML2C_NEGOTIATE_H

#include <libxml/tree.h>

#include "action.h"

class Xml2cNegotiate : public Action {
private:
   unsigned int povType;
   
   unsigned int ipmask;
   unsigned int regmask;
   unsigned int regnum;

   void parseType1(xmlNode *type1);
   void parseType2(xmlNode *type2);

public:
   Xml2cNegotiate(xmlNode *n);
   virtual void generate(FILE *outfile);
   unsigned int getType() {return povType;};
};

class PovSubmit : public Action {
private:
   char *var;
   unsigned int povType;
public:
   PovSubmit(void) : var(NULL) {};
   PovSubmit(xmlNode *n);
   ~PovSubmit();
   virtual void generate(FILE *outfile);
   void setType(unsigned int type) {povType = type;};
};


#endif
