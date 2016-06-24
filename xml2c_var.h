/*
 * Id:             $Id: xml2c_var.h 5410 2015-04-15 04:22:29Z cseagle $
 * Last Updated:   $LastChangedDate: 2015-04-15 04:22:29 +0000 (Wed, 15 Apr 2015) $ 
 */

#ifndef __XML2C_VAR_H
#define __XML2C_VAR_H

#include <sys/time.h>
#include <libxml/tree.h>
#include <stdint.h>
#include <vector>
#include <string>

#include "action.h"

using std::vector;
using std::string;

class Xml2cValue;

class Xml2cVar : public Action {

private:
   static unsigned int varId;

   unsigned int id;   
   string name;
   //value may consist of a sequence of data and var expansions
   vector<Xml2cValue*> values;
   
   //disable copy constructor and assignment
   Xml2cVar(const Xml2cVar &rv) {};
   const Xml2cVar &operator=(const Xml2cVar &rv) {return *this;};
   
public:
   Xml2cVar(xmlNode *n);
   ~Xml2cVar();
   virtual void generate(FILE *outfile);
};


#endif
