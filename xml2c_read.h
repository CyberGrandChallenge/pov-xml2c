/*
 * Id:             $Id: xml2c_read.h 4823 2015-03-28 20:17:30Z cseagle $
 * Last Updated:   $LastChangedDate: 2015-03-28 20:17:30 +0000 (Sat, 28 Mar 2015) $ 
 */

#ifndef __XML2C_READ_H
#define __XML2C_READ_H

#include <sys/time.h>
#include <libxml/tree.h>
#include <stdint.h>
#include <pcre.h>
#include <vector>

using std::vector;

#include "action.h"

class MatchPart;
struct Slice;
struct Regex;

class Xml2cRead : public Action {
private:
   static unsigned int readId;
   unsigned int id;
   
   char *var;
   Slice *slice;
   Regex *varRegex;

   vector<MatchPart*> matchParts;

   vector<uint8_t> *delim;

   bool lengthIsVar;
   unsigned int readLen;
   char *lengthVar;

   unsigned int timeout_val;
   struct timeval timeout;
   int echo;
   bool invert;
   bool povMode;
   
   void doRead(FILE *outfile);

   //disable copy
   Xml2cRead(const Xml2cRead &rr) {};
   const Xml2cRead &operator=(const Xml2cRead &rr) {return *this;}

public:
   Xml2cRead(xmlNode *n, bool echoEnable = true);
   ~Xml2cRead();
   virtual void generate(FILE *conn);
};

#endif
