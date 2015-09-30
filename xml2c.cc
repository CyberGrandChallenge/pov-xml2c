/*
 * Id:             $Id: xml2c.cc 6829 2015-06-12 05:37:32Z cseagle $
 * Last Updated:   $LastChangedDate: 2015-06-12 05:37:32 +0000 (Fri, 12 Jun 2015) $ 
 */
 
/*
 pov-xml2c is a utility for converting xml PoV specifications into C source
 compatible with the DECREE platform.  When linked against provided support
 libraries, the end result is a DECREE executable that performs all of the 
 actions specified in the input xml specification.

 Usage:
    1. Process an xml file conforming to the cgc-pov.dtd
    2. capture the generated C source into projects/src
    3. cd into projects
    4. make
    5. generated executable will be found in projects/bin
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <vector>

using std::vector;

#include "utils.h"
#include "xml2c_delay.h"
#include "xml2c_read.h"
#include "xml2c_write.h"
#include "xml2c_var.h"
#include "xml2c_negotiate.h"

#include "reasons.h"

#define CGC_REPLAY_DTD "/usr/share/cgc-docs/cfe-pov.dtd"

vector<Action*> pov;

static int parseTimeout = 10;

xmlExternalEntityLoader default_XEE_loader;

static void parse_alarm_handler(int) {
   throw (int)PARSE_TIMEOUT;
}

/*
 * Iterate over PoV nodes to generate corresponding source
 */
int generateSource(FILE *outfile) {
   //all the headers we will need
   fprintf(outfile, "#include <libpov.h>\n");
   fprintf(outfile, "int main(void) {\n");

   int povType = 0;

   for (vector<Action*>::iterator i = pov.begin(); i != pov.end(); i++) {
      Action *a = *i;
      Xml2cNegotiate *neg = dynamic_cast<Xml2cNegotiate*>(a);
      if (neg != NULL) {
         povType = neg->getType();
      }
      else {
         PovSubmit *sub = dynamic_cast<PovSubmit*>(a);
         if (sub != NULL) {
            sub->setType(povType);
         }
      }
      
      a->generate(outfile);
   }

   fprintf(outfile, "}\n");
   return 0;
}   

bool buildPoV(xmlNode *pov_xml, bool echoEnable) {
   bool isType2 = false;
   bool hasSubmit = false;
   xmlNode *serviceNode = findChild(pov_xml, "cbid");
   xmlNode *povNode = findChild(pov_xml, "replay");
   //The "seed" element is ignored in PoVs.
   unsigned int len;
   unsigned int errorCount = 0;
   char *text = getNodeText(serviceNode, &len);
   xmlFree(text);
   for (xmlNode *child = povNode->children; child != NULL; child = child->next) {
      char *type = (char*)child->name;
      try {
         if (strcmp(type, "write") == 0) {
            pov.push_back(new Xml2cWrite(child, echoEnable));
         }
         else if (strcmp(type, "read") == 0) {
            pov.push_back(new Xml2cRead(child, echoEnable));
         }
         else if (strcmp(type, "delay") == 0) {
            pov.push_back(new Xml2cDelay(child));
         }
         else if (strcmp(type, "decl") == 0) {
            pov.push_back(new Xml2cVar(child));
         }
         else if (strcmp(type, "negotiate") == 0) {
            Xml2cNegotiate *negotiate = new Xml2cNegotiate(child);
            isType2 = negotiate->getType() == 2;
            pov.push_back(negotiate);
         }
         else if (strcmp(type, "submit") == 0) {
            pov.push_back(new PovSubmit(child));
            hasSubmit = true;
         }
      } catch (int ex) {
         errorCount++;
         if (ex == PARSE_TIMEOUT) {
            throw ex;
         }
      }
   }
   //make certain there is always at least a default submit for type 2 povs
   if (isType2 && !hasSubmit) {
      pov.push_back(new PovSubmit());
   }
   return errorCount == 0;
}   

//assure that we never load external entities
xmlParserInputPtr null_XEE_loader(const char *URL,  const char *ID, xmlParserCtxtPtr context) {
   if (strcmp(URL, CGC_REPLAY_DTD) == 0) {
      return default_XEE_loader(URL, ID, context);
   }
   else {
      return NULL; //don't load any external entities other than our own dtd
   }
}

//added to support analysis
void doDoc(xmlParserCtxtPtr ctxt, int xmlFd, xmlDocPtr *retval){
   /* parse the file, activating the DTD validation option */
   /* disallow network access */

   *retval = xmlCtxtReadFd(ctxt, xmlFd, "", NULL, XML_PARSE_NONET);

   if (*retval != NULL) {
      xmlDtdPtr dtd = xmlParseDTD(NULL, (xmlChar*)CGC_REPLAY_DTD);
      if (dtd == NULL) {
         fprintf(stderr, "Failed to parse DTD\n");
         xmlFreeDoc(*retval);
         *retval = NULL;
      }
      xmlValidCtxtPtr vctxt = xmlNewValidCtxt();
      vctxt->userData = (void *) stderr;
      vctxt->error    = (xmlValidityErrorFunc) fprintf;
      vctxt->warning  = (xmlValidityWarningFunc) fprintf;
      if (xmlValidateDtd(vctxt, *retval, dtd) == 0) {
         fprintf(stderr, "Failed to validate xml against DTD\n");
         xmlFreeDoc(*retval);
         *retval = NULL;
      }
      xmlFreeDtd(dtd);
   }
}

void usage(const char *cmd, int reason) {
   fprintf(stderr, "usage: %s [options] -x xml-file\n", cmd);
   fprintf(stderr, "  -h Display this usage statement\n");
   fprintf(stderr, "  -o Output file name.  Defaults to stdout\n");
   fprintf(stderr, "  -v verify the xml against cfe-pov.dtd\n");
   fprintf(stderr, "  -t Timeout alarm value for parsing xml.\n");
   fprintf(stderr, "  -x xml pov file.\n");
   exit(reason);
}

int main(int argc, char **argv) {
   char *endptr;
   int opt;
   int xmlFd = 0;
   char *outfilename = NULL;
   char *xmlFile = NULL;
   char *parseEnd = NULL;
   bool verifyOnly = false;
   bool echoEnable = false;
   int result = REASON_SUCCESS;

   while ((opt = getopt(argc, argv, "hvt:x:o:")) != -1) {
      switch (opt) {
         case 'v':
            if (verifyOnly) {
               fprintf(stderr, "option -v may be specified only once\n");
               exit(REASON_INVALID_OPT);
            }
            verifyOnly = true;
            break;
         case 'o':
            if (outfilename != NULL) {
               fprintf(stderr, "option -o may be specified only once\n");
               exit(REASON_INVALID_OPT);
            }
            outfilename = optarg;
            break;
         case 'x':
            if (xmlFile) {
               fprintf(stderr, "option -x may be specified only once\n");
               exit(REASON_INVALID_OPT);
            }
            xmlFile = optarg;
            break;
         case 't':
            if (parseEnd != NULL) {
               fprintf(stderr, "option -t may be specified only once\n");
               exit(REASON_INVALID_OPT);
            }
            parseTimeout = strtoul(optarg, &parseEnd, 10);
            if (*parseEnd || parseEnd == optarg) {
               exit(REASON_INVALID_PARSE_TIMEOUT);
            }
            break;
         case 'h':
            usage(argv[0], 0);
            break;
         default:
            fprintf(stderr, "invalid option -%c\n", optopt);
            usage(argv[0], REASON_INVALID_OPT);
      }
   }
   
   if (xmlFile == NULL) {
      fprintf(stderr, "pov-xml2c: xmlFile argument is missing.\n");
      exit(REASON_INVALID_OPT);
   }
   xmlFd = open(xmlFile, O_RDONLY);
   
   lseek(xmlFd, 0, SEEK_SET);

   //Forbid external entities
   default_XEE_loader = xmlGetExternalEntityLoader();
   xmlSetExternalEntityLoader(null_XEE_loader);

   xmlParserCtxtPtr ctxt; /* the parser context */
   xmlDocPtr doc; /* the resulting document tree */

   /* create a parser context */
   ctxt = xmlNewParserCtxt();
   if (ctxt == NULL) {
      fprintf(stderr, "pov-xml2c failed to allocate parser context\n");
      exit(REASON_LIBXML_FAIL);
   }

   signal(SIGALRM, parse_alarm_handler);
   //timeout for parsing XML / regexes
   alarm(parseTimeout);

   try {
   
      /* create the document */
      doDoc(ctxt, xmlFd, &doc);
   
      /* free up the parser context. Do this here to close associated file descriptor */
      xmlFreeParserCtxt(ctxt);
      /* check if parsing suceeded */
      if (doc == NULL) {
         fprintf(stderr, "pov-xml2c failed to parse xml file: %d\n", xmlFd);
         exit(REASON_XML_BAD);
      }
      else {
         /* check if validation suceeded */
         if (ctxt->valid == 0) {
            fprintf(stderr, "pov-xml2c failed to validate xml file\n");
            exit(REASON_XML_DTD_FAIL);
         }
         else {
            xmlNode *pov = xmlDocGetRootElement(doc);
            if (pov != NULL && strcmp((char*)pov->name, "cfepov") == 0) {
               bool success = buildPoV(pov, echoEnable);
               alarm(0);  //cancel alarm for xml parsing
               signal(SIGALRM, SIG_DFL);
               
               /* free up the document */
               xmlFreeDoc(doc);
               if (success) {
                  if (!verifyOnly) {
                     FILE *outfile;
                     if (outfilename != NULL) {
                        outfile = fopen(outfilename, "w");
                        if (outfile == NULL) {
                           fprintf(stderr, "Failed to open output file\n");
                           exit(1);
                        }
                     }
                     else {
                        outfile = stdout;
                     }
                     result = generateSource(outfile);
                     if (outfilename != NULL) {
                        fclose(outfile);
                     }
                  }
               }
               else {
                  fprintf(stderr, "pov-xml2c terminating as a result of PoV specification data error(s).\n");
                  result = REASON_XML_CONTENT;
               }
            }
            else {
               //invalid doc
               fprintf(stderr, "pov-xml2c failed to locate <pov> root node\n");
               result = REASON_XML_CONTENT;
            }
         }
      }
   } catch (int ex) {
      if (ex == PARSE_TIMEOUT) {
         result = REASON_PARSE_TIMEOUT;
      }
   }
   exit(result);
}

