/*
 * Id:             $Id: xml2c_write.h 3725 2015-02-10 23:48:55Z cseagle $
 * Last Updated:   $LastChangedDate: 2015-02-10 23:48:55 +0000 (Tue, 10 Feb 2015) $ 
 */

#ifndef __XML2C_WRITE_H
#define __XML2C_WRITE_H

#include <libxml/tree.h>
#include <stdint.h>
#include <vector>
#include <string>

#include "action.h"

using std::vector;
using std::string;

class Xml2cWrite : public Action {

private:
   static unsigned int writeId;

   unsigned int id;   
   
   vector< vector<uint8_t>* > data;
   vector<string> vars;
   int echo;

   //disable copy
   Xml2cWrite(const Xml2cWrite &rr) {};
   const Xml2cWrite &operator=(const Xml2cWrite &rr) {return *this;}
   
public:
   Xml2cWrite(xmlNode *n, bool echoEnable = true);
   ~Xml2cWrite();
   virtual void generate(FILE *outfile);
};

#endif
