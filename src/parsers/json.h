#ifndef JSON_H
#define JSON_H

#include <cjson/cJSON.h>

enum JSONTypes
{
    JSONTypeInvalid = cJSON_Invalid,
    JSONTypeFalse   = cJSON_False,
    JSONTypeTrue    = cJSON_True,
    JSONTypeNULL    = cJSON_NULL,
    JSONTypeNumber  = cJSON_Number,
    JSONTypeString  = cJSON_String,
    JSONTypeArray   = cJSON_Array,
    JSONTypeObject  = cJSON_Object
};

typedef cJSON JSON;

JSON* JSONParse(const char *value);
JSON* JSONParseWithLength(const char *value, size_t buffer_length);
char* JSONPrint(const JSON *item);
void JSONDelete(JSON *item);

/* Returns the number of items in an array (or object). */
int JSONGetArraySize(const JSON *array);
/* Retrieve item number "index" from array "array". Returns NULL if unsuccessful. */
JSON* JSONGetArrayItem(const JSON *array, int index);
/* Get item "string" from object. Case insensitive. */
JSON* JSONGetObjectItem(const JSON * const object, const char * const string);
JSON* JSONGetObjectItemCaseSensitive(const JSON * const object, const char * const string);
int JSONHasObjectItem(const JSON *object, const char *string);
/* For analysing failed parses. This returns a pointer to the parse error. You'll probably need to look a few chars back to make sense of it. Defined when JSONParse() returns 0. 0 when JSONParse() succeeds. */
const char* JSONGetErrorPtr(void);

/* Check item type and return its value */
char* JSONGetStringValue(const JSON * const item);
double JSONGetNumberValue(const JSON * const item);

/* These functions check the type of an item */
int JSONIsInvalid(const JSON * const item);
int JSONIsFalse(const JSON * const item);
int JSONIsTrue(const JSON * const item);
int JSONIsBool(const JSON * const item);
int JSONIsNull(const JSON * const item);
int JSONIsNumber(const JSON * const item);
int JSONIsString(const JSON * const item);
int JSONIsArray(const JSON * const item);
int JSONIsObject(const JSON * const item);

/* These calls create a JSON item of the appropriate type. */
JSON* JSONCreateNull(void);
JSON* JSONCreateTrue(void);
JSON* JSONCreateFalse(void);
JSON* JSONCreateBool(int boolean);
JSON* JSONCreateNumber(double num);
JSON* JSONCreateString(const char *string);
JSON* JSONCreateArray(void);
JSON* JSONCreateObject(void);

/* Create a string where valuestring references a string so
 * it will not be freed by JSONDelete */
JSON* JSONCreateStringReference(const char *string);
/* Create an object/array that only references it's elements so
 * they will not be freed by JSONDelete */
JSON* JSONCreateObjectReference(const JSON *child);
JSON* JSONCreateArrayReference(const JSON *child);

/* These utilities create an Array of count items.
 * The parameter count cannot be greater than the number of elements in the number array, otherwise array access will be out of bounds.*/
JSON* JSONCreateIntArray(const int *numbers, int count);
JSON* JSONCreateFloatArray(const float *numbers, int count);
JSON* JSONCreateDoubleArray(const double *numbers, int count);
JSON* JSONCreateStringArray(const char *const *strings, int count);

/* Append item to the specified array/object. */
int JSONAddItemToArray(JSON *array, JSON *item);
int JSONAddItemToObject(JSON *object, const char *string, JSON *item);
/* Use this when string is definitely const (i.e. a literal, or as good as), and will definitely survive the JSON object.
 * WARNING: When this function was used, make sure to always check that (item->type & JSONStringIsConst) is zero before
 * writing to `item->string` */
int JSONAddItemToObjectCS(JSON *object, const char *string, JSON *item);
/* Append reference to item to the specified array/object. Use this when you want to add an existing JSON to a new JSON, but don't want to corrupt your existing JSON. */
int JSONAddItemReferenceToArray(JSON *array, JSON *item);
int JSONAddItemReferenceToObject(JSON *object, const char *string, JSON *item);

/* Remove/Detach items from Arrays/Objects. */
JSON* JSONDetachItemViaPointer(JSON *parent, JSON * const item);
JSON* JSONDetachItemFromArray(JSON *array, int which);
void JSONDeleteItemFromArray(JSON *array, int which);
JSON* JSONDetachItemFromObject(JSON *object, const char *string);
JSON* JSONDetachItemFromObjectCaseSensitive(JSON *object, const char *string);
void JSONDeleteItemFromObject(JSON *object, const char *string);
void JSONDeleteItemFromObjectCaseSensitive(JSON *object, const char *string);

/* Update array items. */
int JSONInsertItemInArray(JSON *array, int which, JSON *newitem); /* Shifts pre-existing items to the right. */
int JSONReplaceItemViaPointer(JSON * const parent, JSON * const item, JSON * replacement);
int JSONReplaceItemInArray(JSON *array, int which, JSON *newitem);
int JSONReplaceItemInObject(JSON *object,const char *string,JSON *newitem);
int JSONReplaceItemInObjectCaseSensitive(JSON *object,const char *string,JSON *newitem);

/* Helper functions for creating and adding items to an object at the same time.
 * They return the added item or NULL on failure. */
JSON* JSONAddNullToObject(JSON * const object, const char * const name);
JSON* JSONAddTrueToObject(JSON * const object, const char * const name);
JSON* JSONAddFalseToObject(JSON * const object, const char * const name);
JSON* JSONAddBoolToObject(JSON * const object, const char * const name, const int boolean);
JSON* JSONAddNumberToObject(JSON * const object, const char * const name, const double number);
JSON* JSONAddStringToObject(JSON * const object, const char * const name, const char * const string);
JSON* JSONAddObjectToObject(JSON * const object, const char * const name);
JSON* JSONAddArrayToObject(JSON * const object, const char * const name);

/* When assigning an integer value, it needs to be propagated to valuedouble too. */
#define JSONSetIntValue(object, number) ((object) ? (object)->valueint = (object)->valuedouble = (number) : (number))
/* helper for the JSONSetNumberValue macro */
double JSONSetNumberHelper(JSON *object, double number);
#define JSONSetNumberValue(object, number) ((object != NULL) ? JSONSetNumberHelper(object, doublenumber) : (number))
/* Change the valuestring of a JSONString object, only takes effect when type of object is JSONString */
char* JSONSetValuestring(JSON *object, const char *valuestring);

#define JSONArrayForEach(element, array) for(element = (array != NULL) ? (array)->child : NULL; element != NULL; element = element->next)

#endif