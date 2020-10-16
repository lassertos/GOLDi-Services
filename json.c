#include "json.h"

JSON* JSONParse(const char *value)
{
    return cJSON_Parse(value);
}
JSON* JSONParseWithLength(const char *value, size_t buffer_length)
{
    return cJSON_ParseWithLength(value, buffer_length);
}
char* JSONPrint(const JSON *item)
{
    return cJSON_Print(item);
}
void JSONDelete(JSON *item)
{
    return cJSON_Delete(item);
}

/* Returns the number of items in an array (or object). */
int JSONGetArraySize(const JSON *array)
{
    return cJSON_GetArraySize(array);
}
/* Retrieve item number "index" from array "array". Returns NULL if unsuccessful. */
JSON* JSONGetArrayItem(const JSON *array, int index)
{
    return cJSON_GetArrayItem(array, index);
}
/* Get item "string" from object. Case insensitive. */
JSON* JSONGetObjectItem(const JSON * const object, const char * const string)
{
    return cJSON_GetObjectItem(object, string);
}
JSON* JSONGetObjectItemCaseSensitive(const JSON * const object, const char * const string)
{
    return cJSON_GetObjectItemCaseSensitive(object, string);
}
int JSONHasObjectItem(const JSON *object, const char *string)
{
    return cJSON_HasObjectItem(object, string);
}
/* For analysing failed parses. This returns a pointer to the parse error. You'll probably need to look a few chars back to make sense of it. Defined when JSONParse() returns 0. 0 when JSONParse() succeeds. */
const char* JSONGetErrorPtr(void)
{
    return cJSON_GetErrorPtr();
}

/* Check item type and return its value */
char* JSONGetStringValue(const JSON * const item)
{
    return cJSON_GetStringValue(item);
}
double JSONGetNumberValue(const JSON * const item)
{
    return cJSON_GetNumberValue(item);
}

/* These functions check the type of an item */
int JSONIsInvalid(const JSON * const item)
{
    return cJSON_IsInvalid(item);
}
int JSONIsFalse(const JSON * const item)
{
    return cJSON_IsFalse(item);
}
int JSONIsTrue(const JSON * const item)
{  
    return cJSON_IsTrue(item);
}
int JSONIsBool(const JSON * const item)
{
    return cJSON_IsBool(item);
}
int JSONIsNull(const JSON * const item)
{
    return cJSON_IsNull(item);
}
int JSONIsNumber(const JSON * const item)
{
    return cJSON_IsNumber(item);
}
int JSONIsString(const JSON * const item)
{
    return cJSON_IsString(item);
}
int JSONIsArray(const JSON * const item)
{
    return cJSON_IsArray(item);
}
int JSONIsObject(const JSON * const item)
{
    return cJSON_IsObject(item);
}

/* These calls create a JSON item of the appropriate type. */
JSON* JSONCreateNull(void)
{
    return cJSON_CreateNull();
}
JSON* JSONCreateTrue(void)
{
    return cJSON_CreateTrue();
}
JSON* JSONCreateFalse(void)
{
    return cJSON_CreateFalse();
}
JSON* JSONCreateBool(int boolean)
{
    return cJSON_CreateBool(boolean);
}
JSON* JSONCreateNumber(double num)
{
    return cJSON_CreateNumber(num);
}
JSON* JSONCreateString(const char *string)
{
    return cJSON_CreateString(string);
}
JSON* JSONCreateArray(void)
{
    return cJSON_CreateArray();
}
JSON* JSONCreateObject(void)
{
    return cJSON_CreateObject();
}

/* Create a string where valuestring references a string so
 * it will not be freed by JSONDelete */
JSON* JSONCreateStringReference(const char *string)
{
    return cJSON_CreateStringReference(string);
}
/* Create an object/array that only references it's elements so
 * they will not be freed by JSONDelete */
JSON* JSONCreateObjectReference(const JSON *child)
{
    return cJSON_CreateObjectReference(child);
}
JSON* JSONCreateArrayReference(const JSON *child)
{
    return cJSON_CreateArrayReference(child);
}

/* These utilities create an Array of count items.
 * The parameter count cannot be greater than the number of elements in the number array, otherwise array access will be out of bounds.*/
JSON* JSONCreateIntArray(const int *numbers, int count)
{
    return cJSON_CreateIntArray(numbers, count);
}
JSON* JSONCreateFloatArray(const float *numbers, int count)
{
    return cJSON_CreateFloatArray(numbers, count);
}
JSON* JSONCreateDoubleArray(const double *numbers, int count)
{
    return cJSON_CreateDoubleArray(numbers, count);
}
JSON* JSONCreateStringArray(const char *const *strings, int count)
{
    return cJSON_CreateStringArray(strings, count);
}

/* Append item to the specified array/object. */
int JSONAddItemToArray(JSON *array, JSON *item)
{
    return cJSON_AddItemToArray(array, item);
}
int JSONAddItemToObject(JSON *object, const char *string, JSON *item)
{
    return cJSON_AddItemToObject(object, string, item);
}
/* Use this when string is definitely const (i.e. a literal, or as good as), and will definitely survive the JSON object.
 * WARNING: When this function was used, make sure to always check that (item->type & JSONStringIsConst) is zero before
 * writing to `item->string` */
int JSONAddItemToObjectCS(JSON *object, const char *string, JSON *item)
{
    return cJSON_AddItemToObjectCS(object, string, item);
}
/* Append reference to item to the specified array/object. Use this when you want to add an existing JSON to a new JSON, but don't want to corrupt your existing JSON. */
int JSONAddItemReferenceToArray(JSON *array, JSON *item)
{
    return cJSON_AddItemReferenceToArray(array, item);
}
int JSONAddItemReferenceToObject(JSON *object, const char *string, JSON *item)
{
    return cJSON_AddItemReferenceToObject(object, string, item);
}

/* Remove/Detach items from Arrays/Objects. */
JSON* JSONDetachItemViaPointer(JSON *parent, JSON * const item)
{
    return cJSON_DetachItemViaPointer(parent, item);
}
JSON* JSONDetachItemFromArray(JSON *array, int which)
{
    return cJSON_DetachItemFromArray(array, which);
}
void JSONDeleteItemFromArray(JSON *array, int which)
{
    cJSON_DeleteItemFromArray(array, which);
}
JSON* JSONDetachItemFromObject(JSON *object, const char *string)
{
    return cJSON_DetachItemFromObject(object, string);
}
JSON* JSONDetachItemFromObjectCaseSensitive(JSON *object, const char *string)
{
    return cJSON_DetachItemFromObjectCaseSensitive(object, string);
}
void JSONDeleteItemFromObject(JSON *object, const char *string)
{
    cJSON_DeleteItemFromObject(object, string);
}
void JSONDeleteItemFromObjectCaseSensitive(JSON *object, const char *string)
{
    cJSON_DeleteItemFromObjectCaseSensitive(object, string);
}

/* Update array items. */
int JSONInsertItemInArray(JSON *array, int which, JSON *newitem)
{
    return cJSON_InsertItemInArray(array, which, newitem);
} /* Shifts pre-existing items to the right. */
int JSONReplaceItemViaPointer(JSON * const parent, JSON * const item, JSON * replacement)
{
    return cJSON_ReplaceItemViaPointer(parent, item, replacement);
}
int JSONReplaceItemInArray(JSON *array, int which, JSON *newitem)
{
    return cJSON_ReplaceItemInArray(array, which, newitem);
}
int JSONReplaceItemInObject(JSON *object,const char *string,JSON *newitem)
{
    return cJSON_ReplaceItemInObject(object, string, newitem);
}
int JSONReplaceItemInObjectCaseSensitive(JSON *object,const char *string,JSON *newitem)
{
    return cJSON_ReplaceItemInObjectCaseSensitive(object, string, newitem);
}

/* Helper functions for creating and adding items to an object at the same time.
 * They return the added item or NULL on failure. */
JSON* JSONAddNullToObject(JSON * const object, const char * const name)
{
    return cJSON_AddNullToObject(object, name);
}
JSON* JSONAddTrueToObject(JSON * const object, const char * const name)
{
    return cJSON_AddTrueToObject(object, name);
}
JSON* JSONAddFalseToObject(JSON * const object, const char * const name)
{
    return cJSON_AddFalseToObject(object, name);
}
JSON* JSONAddBoolToObject(JSON * const object, const char * const name, const int boolean)
{
    return cJSON_AddBoolToObject(object, name, boolean);
}
JSON* JSONAddNumberToObject(JSON * const object, const char * const name, const double number)
{
    return cJSON_AddNumberToObject(object, name, number);
}
JSON* JSONAddStringToObject(JSON * const object, const char * const name, const char * const string)
{
    return cJSON_AddStringToObject(object, name, string);
}
JSON* JSONAddObjectToObject(JSON * const object, const char * const name)
{
    return cJSON_AddObjectToObject(object, name);
}
JSON* JSONAddArrayToObject(JSON * const object, const char * const name)
{
    return cJSON_AddArrayToObject(object, name);
}

double JSONSetNumberHelper(JSON *object, double number)
{
    return cJSON_SetNumberHelper(object, number);
}
char* JSONSetValuestring(JSON *object, const char *valuestring)
{
    return cJSON_SetValuestring(object, valuestring);
}