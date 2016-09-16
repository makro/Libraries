/*
 *  Compact SAX Parser
 *
 *  Very small, very fast SAX (Simple Api for Xml) implementation
 *
 *  This library is usefull for reading simple xml files made for example
 *  configuration purposes (which content is known/verified beforehand).
 *
 *
 *  21-Jan-2007  Marko Kallinki  Improvements
 *  02-Nov-2006  Marko Kallinki  Initial version
 *
 */


#include <string.h>
#include "csax_parser.h"


/*
 *  Functional description
 *    Deternimes text encoding, with relative simple methods.
 *    Doesn't cover all cases but this library was intended
 *    to be used for xml which content is known.
 *
 */
static int DetermineEncoding(unsigned char* data)
{
  const int next8  = sizeof(char);
  const int next16 = sizeof(char) * 2;
  int return_code  = SAX_ENCODING_UTF8;

  /* check BOM (http://en.wikipedia.org/wiki/Byte_Order_Mark) */
  if (*data >= 0xFE)
    {
      /* UCS2/UTF-16: FE FF big endian, FF FE little endian */
      if (*(data + next8) == 0xFF)
        {
          return_code = SAX_ENCODING_UCS2BE;
        }
      else if (*(data + next8) == 0xFE)
        {
          if (*(data + next16 + next8) != 0 || *(data + next16 + next16) != 0)
            {
              return_code = SAX_ENCODING_UCS2LE;
            }
          /* else it is UTF-32 little endian, FF FE 00 00 */
          else return_code  = SAX_ENCODING_UNSUPPORTED;
        }
    }
  else if (*data == 0 || *(data + next8) == 0)
    {
      /* check if every other char is zero -> guess big endian UCS-2/UTF-16 */
      if (*data == 0 && *(data + next8) != 0 && *(data + next16) == 0)
        {
          return_code = SAX_ENCODING_UCS2BE;
        }
      /* check if every other char is zero -> guess little endian UCS-2/UTF-16 */
      else if (*data != 0 && *(data + next8) == 0 && *(data + next16) != 0)
        {
          return_code = SAX_ENCODING_UCS2LE;
        }
      /* else it is probably UTF-32 big endian, 00 00 FE FF */
      else return_code  = SAX_ENCODING_UNSUPPORTED;
    }
  /* check if properly marked as utf-8, EF BB BF */
  else if (*data == 0xEF && *(data + next8) == 0xBB && *(data + next16) == 0xBF)
    {
      return_code = SAX_ENCODING_UTF8;
    }
  else /* it is 8bit, so check if it is "ANSI" and not utf-8 */
    {
      char* search = strstr((char*)data, "<?xml");
      if (search)
        {
          char* end = strstr(search, ">");
          if (end)
            {
              char enclose = *end;
              *end = 0; /* reduce the search area length */

              search = strstr(search, "encoding");
              if (search)
                  {
                   search = (search + 10); /* strlen(encoding=") */
                   if (strstr(search, "ISO-8859") || /* ISO-8859-1 */
                       strstr(search, "ASCII") ||    /* US-ASCII */
                       strstr(search, "ANSI") ||
                       strstr(search, "-1252") ||    /* windows-1252 */
                       strstr(search, "ascii") ||
                       strstr(search, "atin") )      /* ISO Latin1 */
                    return_code = SAX_ENCODING_ANSI;
                  }
              *end = enclose;
            }
        }
      /* no '<?xml' tag, so maybe this is not strict xml file (utf-8) */
      else return_code = SAX_ENCODING_ANSI;
    }
  return return_code;
}


/*
 *  Compact SAX Parser
 *
 *  Functional description
 *    Parsers XML input with SAX method.
 *
 *  Parameters:
 *    data                      byte array (disposable RAM array)
 *    size                      size of data
 *    callbacks                 callback function pointer list
 *
 *  Returns (callbacks):
 *    startDocument(encoding)   - informs that document reading starts
 *    endDocument(void)         - informs that document reading ends
 *    startElement("TAG")       - informs new open tag
 *    endElement("TAG","text")  - informs tag closing and text inside
 *    attribute("ATTR","value") - informs attribute with value
 *
 *  Todo: ucs2 (16bit) support
 */
static void CSAXParser(char* data, int size, const SAXCallbacks* callbacks)
{
  /* some internal coding patterns used */
  #define CALLBACK( x )   *data = 0; callbacks->x
  #define CASE_START( x ) case x: { while(--size) {
  #define CASE_END        data++; } data++; break; }
  #define SWITCH( x )     { state = x; break; }

  /* switch cases / state machine */
  enum
    {                    /* state ...<TAG ATTR="value">text</TAG>  */
    SAX_FIND_TAG,        /*   0      <                             */
    SAX_READ_TAG,        /*   1       TAG                          */
    SAX_FIND_ATTRIBUTE,  /*   2           A                        */
    SAX_READ_ATTRIBUTE,  /*   3           ATTR                     */
    SAX_FIND_VALUE,      /*   4               ="                   */
    SAX_READ_VALUE,      /*   5                 value"             */
    SAX_READ_TEXT,       /*   6                       >text<       */
    SAX_READ_END_TAG,    /*   7                             /TAG>  */
    SAX_PASS_COMMENT     /*   8   <!-- comments --> <! ->          */
    };

  /* constants */
  const  int next    = sizeof(char);
  const  int islast  = 1;
  const  int notlast = 0;
  /* variables */
  int    state = SAX_FIND_TAG;
  char*  tag   = NULL;
  char*  attr  = NULL;
  char*  value = NULL;
  char*  text  = NULL;
  size++;

  while(size > 1)
    {
      switch(state)
        {
          CASE_START(SAX_FIND_TAG)                    /* State 0: find tag name start  */
            {
              if (*data == '<')                       /* -yes, <tag...>                */
                {
                  *data = 0; /* ..text< */
                  tag = data + next;
                  if (*tag == '/')                    /* -yes, </tag>                  */
                    {
                      tag = data + next + next;
                      text = NULL;
                      SWITCH(SAX_READ_END_TAG)
                    }
                  else if (*tag == '!')               /* -no,  <!--    comment    -->  */
                    {
                      SWITCH(SAX_PASS_COMMENT)
                    }
                  else if (*tag =='?')                /* -no,  <?..    header          */
                    {
                      SWITCH(SAX_FIND_TAG)
                    }

                  SWITCH(SAX_READ_TAG)
                }
            }
          CASE_END
          CASE_START(SAX_READ_TAG)                    /* State 1: find tag name end    */
            {
              if (*data == ' ')                       /* -yes.  <tag ...               */
                {
                  CALLBACK(startElement)(tag);
                  SWITCH(SAX_FIND_ATTRIBUTE)
                }
              else if (*data == '>')                  /* -yes.  <tag>                  */
                {
                  CALLBACK(startElement)(tag);
                  text = data + next;
                  SWITCH(SAX_READ_TEXT)
                }
              else if (*data == '/')                  /* -yes.  <tag/>                 */
                {
                  CALLBACK(startElement)(tag);
                  CALLBACK(endElement)(tag,NULL);
                  SWITCH(SAX_FIND_TAG)
                }
            }
          CASE_END
          CASE_START(SAX_FIND_ATTRIBUTE)              /* State 2: find attribute start */
            {
              if (*data != ' ')
                {
                  if (*data == '/')                   /* -no. <tag .../>               */
                    {
                      if (attr)
                        {
                          CALLBACK(attribute)(attr, value, islast);
                          attr = NULL;
                        }
                      CALLBACK(endElement)(tag,NULL);
                      SWITCH(SAX_FIND_TAG)
                    }
                  else if (*data == '>')              /* -no. <tag ...>                */
                    {
                      if (attr)
                        {
                          CALLBACK(attribute)(attr, value, islast);
                          attr = NULL;
                        }
                      text = data + next;
                      SWITCH(SAX_READ_TEXT)
                    }
                  else                                /* -yes, <tag attr...            */
                    {
                      if (attr)
                        {
                          data--;
                          CALLBACK(attribute)(attr, value, notlast);
                          data++;
                        }
                      attr = data;
                      SWITCH(SAX_READ_ATTRIBUTE)
                    }
                }
            }
          CASE_END
          CASE_START(SAX_READ_ATTRIBUTE)              /* State 3: find attribute end   */
            {
              if (*data == ' ' || *data == '=')       /* -yes. 'attr ' or 'attr='      */
                {
                  *data = 0;
                  SWITCH(SAX_FIND_VALUE)
                }
            }
          CASE_END
          CASE_START(SAX_FIND_VALUE)                  /* State 4: find value start     */
            {
              if (*data != ' ')
                {
                  if (*data == '"')                   /* -yes. attr="                  */
                    {
                      value = (char*)data + next;
                      SWITCH(SAX_READ_VALUE)
                    }
                  else if (*data != '=')              /* -no,  attr                    */
                    {
                      value = NULL;
                      size++; data--;
                      /* attribute data get, but delay CALLBACK(attribute) */
                      SWITCH(SAX_FIND_ATTRIBUTE)
                    }
                }
            }
          CASE_END
          CASE_START(SAX_READ_VALUE)                  /* State 5: find value end       */
            {
              if (*data == '"')                       /* -yes. attr="value"            */
                {
                  *data = 0;
                  /* attribute data get, but delay CALLBACK(attribute) */
                  SWITCH(SAX_FIND_ATTRIBUTE)
                }
            }
          CASE_END
          CASE_START(SAX_READ_TEXT)                   /* State 6: find new tag start   */
            {
              if (*data == '<')
                {
                  if (*(data + next) == '/')          /* -yes,  text</tag>             */
                    {
                      *data = 0; /* ..text< */
                      tag = data + next + next;
                      SWITCH(SAX_READ_END_TAG)
                    }
                  else                                /* -yes , <tag...>               */
                    {
                      size++; data--;
                      SWITCH(SAX_FIND_TAG)
                    }
                }
            }
          CASE_END
          CASE_START(SAX_READ_END_TAG)                /* State 7: find end of the tag  */
            {
              if (*data == '>')                       /* -yes,  text</tag>             */
                {
                  CALLBACK(endElement)(tag, text);
                  SWITCH(SAX_FIND_TAG);
                }
            }
          CASE_END
          CASE_START(SAX_PASS_COMMENT)                /* State 8: find end of comment  */
              {
              if (*data == '>' && *(data - next) == '-')     /* <! comment -> */
                {
                  SWITCH(SAX_FIND_TAG)
                }
            }
          CASE_END
          default:
              break;
        }
    }
}

/*
 *  Just some sanity checks before parsing
 *
 */
static int DataIntegrityCheck(char* data, int size)
{
  int ret_value = 0;
  /* xml has to have root tag '<?></?>' or header <?xml ?> */
  if (size > 6 && data)
    {
      /* if these fails, then data is not in RAM or size is badly wrong! */
      char read = *(data + size - 1);
      char* write = (char*)(data + size - 1);
      *write = 0;
      *write = read;
      ret_value = 1;
    }
  return ret_value;
}

/*
 *  SAXParser
 *
 *  Functional decription
 *
 *    Safety function before calling CSAXParser
 *    (See CSAXParser above for description)
 */
void SAXParser(char* data, const int size, const SAXCallbacks* Callbacks)
{
  if (Callbacks->startElement &&
      Callbacks->endElement &&
      Callbacks->attribute &&
      DataIntegrityCheck(data,size) )
    {
      int enc = DetermineEncoding((unsigned char*)data);

      if (Callbacks->startDocument)
        Callbacks->startDocument(enc);

      CSAXParser(data, size, Callbacks);

      if (Callbacks->endDocument)
        Callbacks->endDocument();
    }
}


/* ------------------------------------------------------------------------- */
#ifdef UNITTEST_MAIN

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

/*
 *  Unit test material.
 *
 */
static const char unittest_xml[] =
  "<?xml version=\"1.0\" encoding=\"ANSI\"><root>"\
  "<item nonsense  id=\"1\"/><item  id=\"2\" ><node/></item>"\
  "<name>marko</name><name >middlename</name>"\
  "<!-- comment <name>kallinki</name> invisible -->"\
  "<number>.?.<cellphone>+35840804...</cellphone>"\
  "<cellphone >020202</cellphone>.?.</number></root>";

static const char unittest_other[] =
  "<xml version=\"1.0\"><root>"\
  "<cellphone >000</cellphone>.?.</number></root>";

static const unsigned char unittest1_ucs2le[] =
{ (unsigned char)0xFF,(unsigned char)0xFE,'<',0,'?',0,'x',0,'m',0,'l',0, 0x00 };

static const unsigned char unittest1_ucs2be[] =
{ (unsigned char)0xFE,(unsigned char)0xFF,0,'<',0,'?',0,'x',0,'m',0,'l', 0x00 };

static const unsigned char unittest2_ucs2le[] =
{   '<',0,'?',0,'x',0,'m',0,'l',0,' ',0,'?',0,'>',0, 0x00 };

static const unsigned char unittest2_ucs2be[] =
{ 0,'<',0,'?',0,'x',0,'m',0,'l',0,' ',0,'?',0,'>', 0x00 };

static const unsigned char unittest3_ucs4le[] =
{ (unsigned char)0xFF,(unsigned char)0xFE,0,0,0,0,0,0,0, 0x00 };

static const unsigned char unittest3_ucs4be[] =
{ 0,0,(unsigned char)0xFE,(unsigned char)0xFF,0,0,0,0,0, 0x00 };

static const unsigned char unittest4_utf8[]   =
{ (unsigned char)0xEF,(unsigned char)0xBB,(unsigned char)0xBF,'<','?','x','m','l',' ',0x00 };

/*
 *  Callback functions.
 *
 */
static void unittest_startDocument(const int encoding)
{
  printf("\nXML encoding %c",encoding);
}

static void unittest_endDocument(void)
{
  printf("\nEND OF XML\n");
}

static void unittest_startElement(char* element)
{
  printf("\nTAG: %s",element);
  if (!strcmp("name",element))
    {
      printf("            <- \"name\" tag I was looking for.");
      assert(strcmp("nam",element));   /* not "nam" equals   */
      assert(strcmp("names",element)); /* not "names" equals */
      assert(strcmp(element,"nam"));   /* not equals "nam"   */
      assert(strcmp(element,"names")); /* not equals "names" */
    }
}

static void unittest_attribute(char* attrib, char* value, int last)
{
  if (value == NULL)
    printf("\n  ATTR: \"%s\"",attrib);
  else
    printf("\n  ATTR: \"%s\" is \"%s\"",attrib,value);

  if (last) printf(" and last attribute.");
}

static void unittest_endElement(char* element, char* value)
{
  if (value == NULL)
    printf("\nEND:%s\n",element);
  else
    printf("\n  TEXT: \"%s\"\nEND:%s\n",value, element);
}

/*
 *  Callback function list.
 *
 */
static const SAXCallbacks unittest_callback_list =
{
  unittest_startDocument,
  unittest_endDocument,
  unittest_startElement,
  unittest_endElement,
  unittest_attribute
};

/*
 *  Unit test harness for SAX parser.
 *
 */
int main(void)
{
  int   ret;
  char* xml = NULL;

  printf("unittest - csax_parser.c\n");

  /*
   *  Create disposable ram array from rom constant.
   *  Normally one would use data read to ram from file.
   */
#if !defined(WIN32) && !defined(_WIN32)
  xml = strdup(unittest_xml);
#else
  xml = _strdup(unittest_xml);
#endif
  printf("\n%s\n",xml);

  SAXParser(xml, (int)strlen(xml), &unittest_callback_list);

  free(xml);

  /* test encoding recognition */
  ret = DetermineEncoding((unsigned char*)unittest_other);
  assert(ret == SAX_ENCODING_ANSI);

  ret = DetermineEncoding((unsigned char*)unittest1_ucs2le);
  assert(ret == SAX_ENCODING_UCS2LE);

  ret = DetermineEncoding((unsigned char*)unittest1_ucs2be);
  assert(ret == SAX_ENCODING_UCS2BE);

  ret = DetermineEncoding((unsigned char*)unittest2_ucs2le);
  assert(ret == SAX_ENCODING_UCS2LE);

  ret = DetermineEncoding((unsigned char*)unittest2_ucs2be);
  assert(ret == SAX_ENCODING_UCS2BE);

  ret = DetermineEncoding((unsigned char*)unittest3_ucs4le);
  assert(ret == SAX_ENCODING_UNSUPPORTED);

  ret = DetermineEncoding((unsigned char*)unittest3_ucs4be);
  assert(ret == SAX_ENCODING_UNSUPPORTED);

  ret = DetermineEncoding((unsigned char*)unittest4_utf8);
  assert(ret == SAX_ENCODING_UTF8);

  printf("\nunittest - done\n");

  return 0;
}

#endif /* UNITTEST_MAIN */

