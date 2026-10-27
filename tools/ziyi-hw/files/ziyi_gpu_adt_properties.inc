static const uint8_t zg_adt_p0[]={122,105,121,105,44,103,112,117,45,112,114,111,98,101,45,118,49,0};
static const uint8_t zg_adt_p1[]={1,2,6,6};
static const uint8_t zg_adt_p2[]={6,0,0,0};
static const uint8_t zg_adt_p3[]={6,0,0,0};
static const uint8_t zg_adt_p4[]={5,0,0,0};
static const uint8_t zg_adt_p5[]={0,4,0,0};
static const uint8_t zg_adt_p6[]={0,0,0,0};
static const uint8_t zg_adt_p7[]={0,64,3,0};
static const uint8_t zg_adt_p8[]={218,231,172,40,115,34,128,76,253,133,74,96,206,216,232,181,190,69,127,15,114,202,250,178,163,54,169,161,31,49,145,142};
static const uint8_t zg_adt_p9[]={103,99,99,0,103,112,117,99,99,0,103,109,117,0,115,109,109,117,0};
static const uint8_t zg_adt_p10[]={0,0,16,0,0,0,0,0,0,0,9,0,0,0,0,0,0,0,217,3,0,0,0,0,0,160,0,0,0,0,0,0,0,160,214,3,0,0,0,0,0,80,2,0,0,0,0,0,0,0,218,3,0,0,0,0,0,0,2,0,0,0,0,0};
static const uint8_t zg_adt_p11[]={122,105,121,105,45,103,112,117,45,112,114,111,98,101,0};
static const struct zadt_property zg_adt_properties[]={{"compatible",zg_adt_p0,sizeof(zg_adt_p0)},{"source-board-id",zg_adt_p1,sizeof(zg_adt_p1)},{"hlos-context-banks",zg_adt_p2,sizeof(zg_adt_p2)},{"hlos-smr",zg_adt_p3,sizeof(zg_adt_p3)},{"gmu-sid",zg_adt_p4,sizeof(zg_adt_p4)},{"gmu-sid-mask",zg_adt_p5,sizeof(zg_adt_p5)},{"source-handoff-count",zg_adt_p6,sizeof(zg_adt_p6)},{"source-gmu-reg-size",zg_adt_p7,sizeof(zg_adt_p7)},{"source-contract-sha256",zg_adt_p8,sizeof(zg_adt_p8)},{"reg-names",zg_adt_p9,sizeof(zg_adt_p9)},{"reg",zg_adt_p10,sizeof(zg_adt_p10)},{"name",zg_adt_p11,sizeof(zg_adt_p11)}};
#define ZG_ADT_PROPERTY_COUNT 12u
