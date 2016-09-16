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


#ifndef CSAX_PARSER_H
#define CSAX_PARSER_H


/*
 *  Supported text encoding types
 *
 *  "UCS2" - NOTE: Only determines the type, doesn't read it yet!
 *         - 16 bit unicode. (ucs2_strlib.h)
 *         - compability with UTF-16 (only with Basic Multilingual Plane)
 *         - xml headers: <?xml endocing="UTF-16">
 *                        <?xml encoding="UCS-2">
 *
 *  UCS2BE - UCS2    big endian byte order (cmp. #ifdef    BIG_ENDIAN_TYPE)
 *  UCS2LE - UCS2 little endian byte order (cmp. #ifdef LITTLE_ENDIAN_TYPE)
 *
 *  UTF8   - 8 bit, standard encoding for xml.
 *         - can be converted to UCS2 with utf8_2_ucs2 function (utf8_string_lib.h)
 *         - 7 bit ascii can be read as UTF-8
 *         - xml headers: <?xml encoding="UTF-8">
 *
 *  ANSI   - 8 bit, ASCII + Windows codepage 1252, western europe and US (ISO Latin1)
 *         - 7 bit ascii can be read as ANSI
 *         - xml headers: <encoding="ISO-8859-1">
 *                        <encoding="ANSI">
 *                        <encoding="windows-1252">
 *                        <encoding="US-ASCII">
 *                        <encoding="ascii">
 *
 *  This implementation requires that XML syntax should only contain
 *  ascii characters (ascii code < 128), but can contain unicode characters
 *  as attribure values and free text inside the tags
 *
 *  <ascii>
 *    <ascii ascii="unicode"/>unicode</ascii>
 *  </ascii>
 */
enum
  {
  SAX_ENCODING_UNSUPPORTED,
  SAX_ENCODING_UTF8,
  SAX_ENCODING_ANSI,
  SAX_ENCODING_UCS2BE,
  SAX_ENCODING_UCS2LE
  } sax_encoding_types_t;


/*
 *  Function prototypes for SAX callback functions
 *
 */
typedef void (*startDocumentSAXFunc) ( int encoding );
typedef void (*endDocumentSAXFunc)   ( void );
typedef void (*startElementSAXFunc)  ( char* element );
typedef void (*endElementSAXFunc)    ( char* element, char* value );
typedef void (*attributeSAXFunc)     ( char* attrib,  char* value, int last );

/*
 *  Callback function list structure (to be passed to SAXParser)
 *
 */
typedef struct
{
  startDocumentSAXFunc  startDocument;
  endDocumentSAXFunc    endDocument;
  startElementSAXFunc   startElement;
  endElementSAXFunc     endElement;
  attributeSAXFunc      attribute;
} SAXCallbacks;


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
 *    startDocument(0)          - informs that document reading starts
 *    endDocument(void)         - informs that document reading ends
 *    startElement("TAG")       - informs new open tag
 *    endElement("TAG","text")  - informs tag closing and text inside
 *    attribute("ATTR","value") - informs attribute with value
 *
 *  Todo: ucs2 (16bit) support
 */
void SAXParser(char* data, const int size, const SAXCallbacks* SAXCallbacks);


#endif /* CSAX_PARSER_H */

