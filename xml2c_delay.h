/*
 * Id:             $Id: xml2c_delay.h 3725 2015-02-10 23:48:55Z cseagle $
 * Last Updated:   $LastChangedDate: 2015-02-10 23:48:55 +0000 (Tue, 10 Feb 2015) $ 
 */

#ifndef __XML2C_DELAY_H
#define __XML2C_DELAY_H

#include <libxml/tree.h>

#include "action.h"

class Xml2cDelay : public Action {
private:
   unsigned int msec;

public:
   Xml2cDelay(xmlNode *n);
   virtual void generate(FILE *outfile);
};


#endif
