/*
 * Id:             $Id: action.h 3725 2015-02-10 23:48:55Z cseagle $
 * Last Updated:   $LastChangedDate: 2015-02-10 23:48:55 +0000 (Tue, 10 Feb 2015) $ 
 */

#ifndef __ACTION_H
#define __ACTION_H

#include <stdio.h>

enum {
   ECHO_NO,
   ECHO_YES,
   ECHO_ASCII
};

class Action {

public:
   virtual void generate(FILE *outfile) = 0;

};

#endif
