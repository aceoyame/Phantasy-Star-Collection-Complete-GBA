typedef struct{const char*title;uint32_t table,size,flags;} Cart;
#define CATALOG_COUNT 10u
static const Cart catalog[CATALOG_COUNT]={
{"Phantasy Star Adventure",137706368u,131072u,1u},
{"Phantasy Star Gaiden",166669648u,262144u,17u},
{"Anne Adventure",137952128u,262144u,4u},
{"Rudger Adventure",137952384u,262144u,4u},
{"Kinds Adventure",137952640u,262144u,4u},
{"Shilka Adventure",137952896u,262144u,4u},
{"Amia Adventure",167382352u,262144u,4u},
{"Huey Adventure",167472720u,262144u,4u},
{"Eusis Adventure",167558992u,262144u,4u},
{"Nei Adventure",167667280u,262144u,4u},
};
#define menu_background ((const unsigned char*)167559248u)
