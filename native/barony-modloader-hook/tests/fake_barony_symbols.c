/* Test-only ELF symbol provider for native hook smoke tests. */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
int bml_fake_detour_counter __asm__("bml_fake_detour_counter") __attribute__((visibility("protected"))) = 0;

typedef struct BmlFakeNode {
    struct BmlFakeNode *next;
    struct BmlFakeNode *prev;
    struct BmlFakeList *list;
    void *element;
    void (*deconstructor)(void *data);
    uint32_t size;
} BmlFakeNode;

typedef struct BmlFakeList {
    BmlFakeNode *first;
    BmlFakeNode *last;
} BmlFakeList;

typedef struct BmlFakeItem {
    int type;
    int status;
    int16_t beatitude;
    int16_t count;
    uint32_t appearance;
    bool identified;
    uint32_t uid;
    int32_t x;
    int32_t y;
    uint32_t ownerUid;
    uint32_t interactNPCUid;
    bool forcedPickupByPlayer;
    bool isDroppable;
    bool playerSoldItemToShop;
    bool itemHiddenFromShop;
    bool notifyIcon;
    bool spellNotifyIcon;
    uint8_t itemRequireTradingSkillInShop;
    bool itemSpecialShopConsumable;
    BmlFakeNode *node;
} BmlFakeItem;

typedef struct BmlFakeStat {
    unsigned char prefix[0x9e8];
    BmlFakeList void_chest_inventory;
} BmlFakeStat;

typedef struct BmlFakeMapPrefix {
    char name[32];
    char author[32];
    unsigned int width;
    unsigned int height;
    unsigned int skybox;
    int32_t flags[16];
    int32_t *tiles;
    unsigned char entities_map_padding[56];
    BmlFakeList *entities;
    BmlFakeList *creatures;
} BmlFakeMapPrefix;

#define BML_FAKE_ENTITY_SIZE 5024U
#define BML_FAKE_ENTITY_UID_OFFSET 104U
#define BML_FAKE_ENTITY_X_OFFSET 208U
#define BML_FAKE_ENTITY_Y_OFFSET 216U
#define BML_FAKE_ENTITY_Z_OFFSET 224U
#define BML_FAKE_ENTITY_SPRITE_OFFSET 312U
#define BML_FAKE_ENTITY_SKILL_OFFSET 640U
#define BML_FAKE_ENTITY_CHILDREN_OFFSET 920U
#define BML_FAKE_ENTITY_PARENT_OFFSET 936U
#define BML_FAKE_ENTITY_MYNODE_OFFSET 4880U
#define BML_FAKE_ENTITY_BEHAVIOR_OFFSET 4936U
#define BML_FAKE_ENTITY_RANBEHAVIOR_OFFSET 4944U
#define BML_FAKE_CHEST_VOID_STATE_SKILL_INDEX 17U
#define BML_FAKE_CHEST_SPRITE 1791
#define BML_FAKE_CHEST_LID_SPRITE 1790
#define BML_FAKE_INTERNAL_MARKER_SKILL58 ((int32_t)0x424D4C00)
void *bml_fake_selected_entity[4] __asm__("selectedEntity") = { NULL, NULL, NULL, NULL };

static void bml_fake_write_u32(void *base, size_t offset, uint32_t value) {
    memcpy((unsigned char *)base + offset, &value, sizeof(value));
}

static void bml_fake_write_i32(void *base, size_t offset, int32_t value) {
    memcpy((unsigned char *)base + offset, &value, sizeof(value));
}

static int32_t bml_fake_read_i32(const void *base, size_t offset) {
    int32_t value = 0;
    memcpy(&value, (const unsigned char *)base + offset, sizeof(value));
    return value;
}

static void bml_fake_write_double(void *base, size_t offset, double value) {
    memcpy((unsigned char *)base + offset, &value, sizeof(value));
}

static void bml_fake_write_ptr(void *base, size_t offset, void *value) {
    memcpy((unsigned char *)base + offset, &value, sizeof(value));
}

static void *bml_fake_function_address(void (*function)(void)) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}


static BmlFakeStat bml_fake_stat_zero;
void *bml_fake_stats[1] __asm__("stats") = { &bml_fake_stat_zero };

const char *bml_fake_language_get_impl(int language_id) {
    if (language_id == 4005) {
        return "Open chest";
    }
    return "Fake language";
}

void bml_fake_languageGet(void) __asm__("_ZN8Language3getEi");

__asm__(
    ".text\n"
    ".globl _ZN8Language3getEi\n"
    ".type _ZN8Language3getEi, @function\n"
    "_ZN8Language3getEi:\n"
    "  push %rbp\n"
    "  mov %rsp, %rbp\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  call bml_fake_language_get_impl@PLT\n"
    "  pop %rbp\n"
    "  ret\n"
    ".size _ZN8Language3getEi, .-_ZN8Language3getEi\n");

static void bml_fake_item_deconstructor(void *data) {
    free(data);
}

BmlFakeNode *bml_fake_list_AddNodeLast(BmlFakeList *list) __asm__("_Z16list_AddNodeLastP6list_t");
BmlFakeNode *bml_fake_list_AddNodeLast(BmlFakeList *list) {
    BmlFakeNode *node = (BmlFakeNode *)calloc(1, sizeof(BmlFakeNode));
    if (node == NULL || list == NULL) {
        free(node);
        return NULL;
    }
    node->list = list;
    if (list->last != NULL) {
        list->last->next = node;
        node->prev = list->last;
    } else {
        list->first = node;
    }
    list->last = node;
    return node;
}

BmlFakeNode *bml_fake_list_AddNodeFirst(BmlFakeList *list) __asm__("_Z17list_AddNodeFirstP6list_t");
BmlFakeNode *bml_fake_list_AddNodeFirst(BmlFakeList *list) {
    BmlFakeNode *node = (BmlFakeNode *)calloc(1, sizeof(BmlFakeNode));
    if (node == NULL || list == NULL) {
        free(node);
        return NULL;
    }
    node->list = list;
    if (list->first != NULL) {
        list->first->prev = node;
        node->next = list->first;
    } else {
        list->last = node;
    }
    list->first = node;
    return node;
}

void bml_fake_list_RemoveNode(BmlFakeNode *node) __asm__("_Z15list_RemoveNodeP6node_t");
void bml_fake_list_RemoveNode(BmlFakeNode *node) {
    if (node == NULL || node->list == NULL) {
        return;
    }
    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        node->list->first = node->next;
    }
    if (node->next != NULL) {
        node->next->prev = node->prev;
    } else {
        node->list->last = node->prev;
    }
    if (node->deconstructor != NULL && node->element != NULL) {
        node->deconstructor(node->element);
    }
    free(node);
}

void bml_fake_list_FreeAll(BmlFakeList *list) __asm__("_Z12list_FreeAllP6list_t");
void bml_fake_list_FreeAll(BmlFakeList *list) {
    while (list != NULL && list->first != NULL) {
        bml_fake_list_RemoveNode(list->first);
    }
}

void *bml_fake_newItem(int type, int status, int16_t beatitude, int16_t count, uint32_t appearance, bool identified, BmlFakeList *inventory) __asm__("_Z7newItem8ItemType6StatusssjbP6list_t");
void *bml_fake_newItem(int type, int status, int16_t beatitude, int16_t count, uint32_t appearance, bool identified, BmlFakeList *inventory) {
    BmlFakeItem *item = (BmlFakeItem *)calloc(1, sizeof(BmlFakeItem));
    if (item == NULL) {
        return NULL;
    }
    item->type = type;
    item->status = status;
    item->beatitude = beatitude;
    item->count = count;
    item->appearance = appearance;
    item->identified = identified;
    item->isDroppable = true;
    if (inventory != NULL) {
        item->node = bml_fake_list_AddNodeLast(inventory);
        if (item->node == NULL) {
            free(item);
            return NULL;
        }
        item->node->element = item;
        item->node->deconstructor = bml_fake_item_deconstructor;
    }
    return item;
}

void *bml_fake_add_item_to_void_chest_server_impl(void *entity, int player, void *item, bool force_new_stack, void *picked_up_stack) {
    (void)entity;
    (void)player;
    (void)force_new_stack;
    (void)picked_up_stack;
    BmlFakeItem *fake_item = (BmlFakeItem *)item;
    if (fake_item == NULL) {
        return (void *)(uintptr_t)42U;
    }
    if (fake_item->node == NULL) {
        fake_item->node = bml_fake_list_AddNodeLast(&bml_fake_stat_zero.void_chest_inventory);
        if (fake_item->node == NULL) {
            return NULL;
        }
        fake_item->node->element = fake_item;
        fake_item->node->deconstructor = bml_fake_item_deconstructor;
    }
    return fake_item;
}

bool bml_fake_remove_item_from_void_chest_server_impl(void *entity, int player, void *item, int count) {
    (void)entity;
    (void)player;
    (void)count;
    BmlFakeItem *fake_item = (BmlFakeItem *)item;
    if (fake_item == NULL || fake_item->node == NULL) {
        return false;
    }
    bml_fake_list_RemoveNode(fake_item->node);
    return true;
}

void *bml_fake_add_item_to_chest_impl(void *entity, void *item, bool force_new_stack, void *specific_destination_stack) {
    (void)specific_destination_stack;
    return bml_fake_add_item_to_void_chest_server_impl(entity, 0, item, force_new_stack, NULL);
}

void *bml_fake_get_item_from_chest_impl(void *entity, void *item, int amount, bool get_info_only) {
    (void)entity;
    (void)amount;
    BmlFakeItem *fake_item = (BmlFakeItem *)item;
    if (fake_item == NULL) {
        return NULL;
    }
    if (!get_info_only && fake_item->node != NULL) {
        BmlFakeNode *node = fake_item->node;
        fake_item->node = NULL;
        node->deconstructor = NULL;
        bml_fake_list_RemoveNode(node);
    }
    return fake_item;
}

int bml_fake_detour_target(void);

__asm__(
    ".text\n"
    ".globl bml_fake_detour_target\n"
    ".type bml_fake_detour_target, @function\n"
    "bml_fake_detour_target:\n"
    "  xor %eax, %eax\n"
    "  cmpl $-1, bml_fake_detour_counter(%rip)\n"
    "  je 1f\n"
    "  test %eax, %eax\n"
    "  je 2f\n"
    "1:\n"
    "  mov $99, %eax\n"
    "2:\n"
    "  push %rbp\n"
    "  mov %rsp, %rbp\n"
    "  incl bml_fake_detour_counter(%rip)\n"
    "  mov $41, %eax\n"
    "  pop %rbp\n"
    "  ret\n"
    ".size bml_fake_detour_target, .-bml_fake_detour_target\n");

void bml_fake_actChest(void) __asm__("_Z8actChestP6Entity");

__asm__(
    ".text\n"
    ".globl _Z8actChestP6Entity\n"
    ".type _Z8actChestP6Entity, @function\n"
    "_Z8actChestP6Entity:\n"
    "  push %rbp\n"
    "  mov %rsp, %rbp\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  pop %rbp\n"
    "  ret\n"
    ".size _Z8actChestP6Entity, .-_Z8actChestP6Entity\n");

void bml_fake_addItemToVoidChestServer(void) __asm__("_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_");

__asm__(
    ".text\n"
    ".globl _ZN6Entity24addItemToVoidChestServerEiP4ItembS1_\n"
    ".type _ZN6Entity24addItemToVoidChestServerEiP4ItembS1_, @function\n"
    "_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_:\n"
    "  push %r15\n"
    "  push %r14\n"
    "  push %r13\n"
    "  push %r12\n"
    "  push %rbp\n"
    "  push %rbx\n"
    "  sub $0x18, %rsp\n"
    "  add $0x18, %rsp\n"
    "  pop %rbx\n"
    "  pop %rbp\n"
    "  pop %r12\n"
    "  pop %r13\n"
    "  pop %r14\n"
    "  pop %r15\n"
    "  call bml_fake_add_item_to_void_chest_server_impl@PLT\n"
    "  ret\n"
    ".size _ZN6Entity24addItemToVoidChestServerEiP4ItembS1_, .-_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_\n");

void bml_fake_actChest(void) __asm__("_Z8actChestP6Entity");
void bml_fake_actChestLid(void) __asm__("_Z11actChestLidP6Entity");

int bml_fake_new_entity_calls __asm__("bml_fake_new_entity_calls") = 0;
int bml_fake_new_entity_sprite_188_calls __asm__("bml_fake_new_entity_sprite_188_calls") = 0;
int bml_fake_new_entity_sprite_1484_calls __asm__("bml_fake_new_entity_sprite_1484_calls") = 0;
int bml_fake_new_entity_sprite_1790_calls __asm__("bml_fake_new_entity_sprite_1790_calls") = 0;
int bml_fake_new_entity_sprite_1791_calls __asm__("bml_fake_new_entity_sprite_1791_calls") = 0;
int bml_fake_new_entity_last_sprite __asm__("bml_fake_new_entity_last_sprite") = 0;
uint32_t bml_fake_new_entity_last_pos __asm__("bml_fake_new_entity_last_pos") = 0U;
void *bml_fake_new_entity_last_entity __asm__("bml_fake_new_entity_last_entity") = NULL;
void *bml_fake_new_entity_last_entity_list __asm__("bml_fake_new_entity_last_entity_list") = NULL;
int bml_fake_entity_list_count __asm__("bml_fake_entity_list_count") = 0;
uint32_t bml_fake_next_entity_uid __asm__("bml_fake_next_entity_uid") = 1000U;
static void *bml_fake_created_entities[128];
static size_t bml_fake_created_entity_count = 0U;

static void bml_fake_entity_deconstructor(void *data) {
    free(data);
}

void *bml_fake_new_entity_impl(int sprite, uint32_t pos, BmlFakeList *entity_list, BmlFakeList *creature_list) {
    unsigned char *entity = (unsigned char *)calloc(1, BML_FAKE_ENTITY_SIZE);
    BmlFakeNode *node;
    (void)creature_list;
    if (entity == NULL) {
        return NULL;
    }

    bml_fake_new_entity_calls += 1;
    bml_fake_new_entity_last_sprite = sprite;
    bml_fake_new_entity_last_pos = pos;
    bml_fake_new_entity_last_entity = entity;
    bml_fake_new_entity_last_entity_list = entity_list;
    if (sprite == 188) {
        bml_fake_new_entity_sprite_188_calls += 1;
    } else if (sprite == 1484) {
        bml_fake_new_entity_sprite_1484_calls += 1;
    } else if (sprite == BML_FAKE_CHEST_LID_SPRITE) {
        bml_fake_new_entity_sprite_1790_calls += 1;
    } else if (sprite == BML_FAKE_CHEST_SPRITE) {
        bml_fake_new_entity_sprite_1791_calls += 1;
    }

    bml_fake_write_u32(entity, BML_FAKE_ENTITY_UID_OFFSET, bml_fake_next_entity_uid++);
    bml_fake_write_double(entity, BML_FAKE_ENTITY_X_OFFSET, (double)(pos % 256U));
    bml_fake_write_double(entity, BML_FAKE_ENTITY_Y_OFFSET, (double)((pos / 256U) % 256U));
    bml_fake_write_double(entity, BML_FAKE_ENTITY_Z_OFFSET, 0.0);
    bml_fake_write_i32(entity, BML_FAKE_ENTITY_SPRITE_OFFSET, sprite);
    if (bml_fake_created_entity_count < (sizeof(bml_fake_created_entities) / sizeof(bml_fake_created_entities[0]))) {
        bml_fake_created_entities[bml_fake_created_entity_count++] = entity;
    }

    if (entity_list != NULL) {
        node = bml_fake_list_AddNodeLast(entity_list);
        if (node == NULL) {
            free(entity);
            return NULL;
        }
        node->element = entity;
        node->deconstructor = bml_fake_entity_deconstructor;
        bml_fake_write_ptr(entity, BML_FAKE_ENTITY_MYNODE_OFFSET, node);
        bml_fake_entity_list_count += 1;
    }

    return entity;
}

void bml_fake_newEntity(void) __asm__("_Z9newEntityijP6list_tS0_");

__asm__(
    ".text\n"
    ".globl _Z9newEntityijP6list_tS0_\n"
    ".type _Z9newEntityijP6list_tS0_, @function\n"
    "_Z9newEntityijP6list_tS0_:\n"
    "  push %rbp\n"
    "  mov %rsp, %rbp\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  call bml_fake_new_entity_impl@PLT\n"
    "  pop %rbp\n"
    "  ret\n"
    ".size _Z9newEntityijP6list_tS0_, .-_Z9newEntityijP6list_tS0_\n");

void bml_fake_actChestLid(void) __asm__("_Z11actChestLidP6Entity");

__asm__(
    ".text\n"
    ".globl _Z11actChestLidP6Entity\n"
    ".type _Z11actChestLidP6Entity, @function\n"
    "_Z11actChestLidP6Entity:\n"
    "  push %rbp\n"
    "  push %rbx\n"
    "  mov %rdi, %rbx\n"
    "  sub $0x8, %rsp\n"
    "  mov 0x3a8(%rdi), %edi\n"
    "  add $0x8, %rsp\n"
    "  pop %rbx\n"
    "  pop %rbp\n"
    "  ret\n"
    ".size _Z11actChestLidP6Entity, .-_Z11actChestLidP6Entity\n");

void bml_fake_getChestInventoryList(void) __asm__("_ZN6Entity21getChestInventoryListEv");

__asm__(
    ".text\n"
    ".globl _ZN6Entity21getChestInventoryListEv\n"
    ".type _ZN6Entity21getChestInventoryListEv, @function\n"
    "_ZN6Entity21getChestInventoryListEv:\n"
    "  push %rbp\n"
    "  mov %rsp, %rbp\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  lea bml_fake_stat_zero+0x9e8(%rip), %rax\n"
    "  pop %rbp\n"
    "  ret\n"
    ".size _ZN6Entity21getChestInventoryListEv, .-_ZN6Entity21getChestInventoryListEv\n");

void bml_fake_removeItemFromVoidChestServer(void) __asm__("_ZN6Entity29removeItemFromVoidChestServerEiP4Itemi");

__asm__(
    ".text\n"
    ".globl _ZN6Entity29removeItemFromVoidChestServerEiP4Itemi\n"
    ".type _ZN6Entity29removeItemFromVoidChestServerEiP4Itemi, @function\n"
    "_ZN6Entity29removeItemFromVoidChestServerEiP4Itemi:\n"
    "  push %rbp\n"
    "  mov %rsp, %rbp\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  call bml_fake_remove_item_from_void_chest_server_impl@PLT\n"
    "  pop %rbp\n"
    "  ret\n"
    ".size _ZN6Entity29removeItemFromVoidChestServerEiP4Itemi, .-_ZN6Entity29removeItemFromVoidChestServerEiP4Itemi\n");

void bml_fake_closeChest(void) __asm__("_ZN6Entity10closeChestEv");

__asm__(
    ".text\n"
    ".globl _ZN6Entity10closeChestEv\n"
    ".type _ZN6Entity10closeChestEv, @function\n"
    "_ZN6Entity10closeChestEv:\n"
    "  push %rbp\n"
    "  mov %rsp, %rbp\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  pop %rbp\n"
    "  ret\n"
    ".size _ZN6Entity10closeChestEv, .-_ZN6Entity10closeChestEv\n");

void bml_fake_closeChestServer(void) __asm__("_ZN6Entity16closeChestServerEv");

__asm__(
    ".text\n"
    ".globl _ZN6Entity16closeChestServerEv\n"
    ".type _ZN6Entity16closeChestServerEv, @function\n"
    "_ZN6Entity16closeChestServerEv:\n"
    "  push %rbp\n"
    "  mov %rsp, %rbp\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  pop %rbp\n"
    "  ret\n"
    ".size _ZN6Entity16closeChestServerEv, .-_ZN6Entity16closeChestServerEv\n");

void bml_fake_addItemToChest(void) __asm__("_ZN6Entity14addItemToChestEP4ItembS1_");

__asm__(
    ".text\n"
    ".type bml_fake_addItemToChest_part0, @function\n"
    "bml_fake_addItemToChest_part0:\n"
    "  push %rbp\n"
    "  mov %rsp, %rbp\n"
    "  call bml_fake_add_item_to_chest_impl@PLT\n"
    "  pop %rbp\n"
    "  ret\n"
    ".size bml_fake_addItemToChest_part0, .-bml_fake_addItemToChest_part0\n"
    ".globl _ZN6Entity14addItemToChestEP4ItembS1_\n"
    ".type _ZN6Entity14addItemToChestEP4ItembS1_, @function\n"
    "_ZN6Entity14addItemToChestEP4ItembS1_:\n"
    "  test %rsi, %rsi\n"
    "  je 1f\n"
    "  movzbl %dl, %edx\n"
    "  jmp bml_fake_addItemToChest_part0\n"
    "  nopl (%rax)\n"
    "1:\n"
    "  xor %eax, %eax\n"
    "  ret\n"
    ".size _ZN6Entity14addItemToChestEP4ItembS1_, .-_ZN6Entity14addItemToChestEP4ItembS1_\n");

void bml_fake_getItemFromChest(void) __asm__("_ZN6Entity16getItemFromChestEP4Itemib");

__asm__(
    ".text\n"
    ".globl _ZN6Entity16getItemFromChestEP4Itemib\n"
    ".type _ZN6Entity16getItemFromChestEP4Itemib, @function\n"
    "_ZN6Entity16getItemFromChestEP4Itemib:\n"
    "  push %rbp\n"
    "  mov %rsp, %rbp\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  call bml_fake_get_item_from_chest_impl@PLT\n"
    "  pop %rbp\n"
    "  ret\n"
    ".size _ZN6Entity16getItemFromChestEP4Itemib, .-_ZN6Entity16getItemFromChestEP4Itemib\n");

void bml_fake_generateDungeon(void) __asm__("_Z15generateDungeonPcjSt5tupleIJiiiiEE");

__asm__(
    ".text\n"
    ".globl _Z15generateDungeonPcjSt5tupleIJiiiiEE\n"
    ".type _Z15generateDungeonPcjSt5tupleIJiiiiEE, @function\n"
    "_Z15generateDungeonPcjSt5tupleIJiiiiEE:\n"
    "  push %rbp\n"
    "  mov %rsp, %rbp\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  mov $7, %eax\n"
    "  pop %rbp\n"
    "  ret\n"
    ".size _Z15generateDungeonPcjSt5tupleIJiiiiEE, .-_Z15generateDungeonPcjSt5tupleIJiiiiEE\n");

static BmlFakeList bml_fake_tile_entity_list_storage = { NULL, NULL };
static BmlFakeList bml_fake_map_entity_list_storage = { NULL, NULL };
static bool bml_fake_shoparea_storage[64U * 48U] = {
    [8U + 8U * 48U] = true,
};
void *bml_fake_TileEntityList __asm__("TileEntityList") = &bml_fake_tile_entity_list_storage;
static void *bml_fake_find_created_entity_by_uid(int uid) {
    for (size_t index = 0U; index < bml_fake_created_entity_count; ++index) {
        void *entity = bml_fake_created_entities[index];
        if (entity != NULL && bml_fake_read_i32(entity, BML_FAKE_ENTITY_UID_OFFSET) == uid) {
            return entity;
        }
    }
    for (size_t index = 0U; index < (sizeof(bml_fake_selected_entity) / sizeof(bml_fake_selected_entity[0])); ++index) {
        void *entity = bml_fake_selected_entity[index];
        if (entity != NULL && bml_fake_read_i32(entity, BML_FAKE_ENTITY_UID_OFFSET) == uid) {
            return entity;
        }
    }
    return NULL;
}

void *bml_fake_uid_to_entity_impl(int uid) {
    return bml_fake_find_created_entity_by_uid(uid);
}

void bml_fake_uidToEntity(void) __asm__("_Z11uidToEntityi");

__asm__(
    ".text\n"
    ".globl _Z11uidToEntityi\n"
    ".type _Z11uidToEntityi, @function\n"
    "_Z11uidToEntityi:\n"
    "  push %rbp\n"
    "  mov %rsp, %rbp\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  call bml_fake_uid_to_entity_impl@PLT\n"
    "  pop %rbp\n"
    "  ret\n"
    ".size _Z11uidToEntityi, .-_Z11uidToEntityi\n");


void bml_fake_assign_actions_impl(void *map_argument) {
    (void)map_argument;
    (void)bml_fake_new_entity_impl(188, 3U, &bml_fake_tile_entity_list_storage, NULL);
}

void bml_fake_assignActions(void) __asm__("_Z13assignActionsP5map_t");

__asm__(
    ".text\n"
    ".globl _Z13assignActionsP5map_t\n"
    ".type _Z13assignActionsP5map_t, @function\n"
    "_Z13assignActionsP5map_t:\n"
    "  push %rbp\n"
    "  mov %rsp, %rbp\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  call bml_fake_assign_actions_impl@PLT\n"
    "  pop %rbp\n"
    "  ret\n"
    ".size _Z13assignActionsP5map_t, .-_Z13assignActionsP5map_t\n");

int bml_fake_set_sprite_attributes_calls __asm__("bml_fake_set_sprite_attributes_calls") = 0;
void *bml_fake_set_sprite_attributes_last_entity __asm__("bml_fake_set_sprite_attributes_last_entity") = NULL;
void *bml_fake_set_sprite_attributes_last_parent __asm__("bml_fake_set_sprite_attributes_last_parent") = NULL;

void bml_fake_set_sprite_attributes_impl(void *entity, void *source, void *parent) {
    int32_t sprite;
    (void)source;
    bml_fake_set_sprite_attributes_calls += 1;
    bml_fake_set_sprite_attributes_last_entity = entity;
    bml_fake_set_sprite_attributes_last_parent = parent;
    if (entity == NULL) {
        return;
    }
    if (parent != NULL) {
        bml_fake_write_ptr(entity, BML_FAKE_ENTITY_PARENT_OFFSET, parent);
    }
    sprite = bml_fake_read_i32(entity, BML_FAKE_ENTITY_SPRITE_OFFSET);
    if (sprite == BML_FAKE_CHEST_SPRITE) {
        bml_fake_write_ptr(entity, BML_FAKE_ENTITY_BEHAVIOR_OFFSET, bml_fake_function_address(bml_fake_actChest));
        bml_fake_write_i32(entity, BML_FAKE_ENTITY_SKILL_OFFSET + (BML_FAKE_CHEST_VOID_STATE_SKILL_INDEX * sizeof(int32_t)), -1);
    } else if (sprite == BML_FAKE_CHEST_LID_SPRITE) {
        bml_fake_write_ptr(entity, BML_FAKE_ENTITY_BEHAVIOR_OFFSET, bml_fake_function_address(bml_fake_actChestLid));
    }
}

void bml_fake_setSpriteAttributes(void) __asm__("_Z19setSpriteAttributesP6EntityS0_S0_");

__asm__(
    ".text\n"
    ".globl _Z19setSpriteAttributesP6EntityS0_S0_\n"
    ".type _Z19setSpriteAttributesP6EntityS0_S0_, @function\n"
    "_Z19setSpriteAttributesP6EntityS0_S0_:\n"
    "  push %rbp\n"
    "  mov %rsp, %rbp\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  call bml_fake_set_sprite_attributes_impl@PLT\n"
    "  pop %rbp\n"
    "  ret\n"
    ".size _Z19setSpriteAttributesP6EntityS0_S0_, .-_Z19setSpriteAttributesP6EntityS0_S0_\n");

BmlFakeMapPrefix bml_fake_map __asm__("map") = {
    "fake-lobby",
    "bml-fake",
    64U,
    48U,
    0U,
    {0},
    NULL,
    {0},
    &bml_fake_map_entity_list_storage,
    NULL,
};
BmlFakeMapPrefix bml_fake_shop_map __asm__("bml_fake_shop_map") = {
    "fake-shop",
    "bml-fake",
    64U,
    48U,
    0U,
    {0},
    NULL,
    {0},
    &bml_fake_map_entity_list_storage,
    NULL,
};
void *bml_fake_assign_actions_map __asm__("bml_fake_assign_actions_map") = &bml_fake_map;
int bml_fake_map_rng __asm__("map_rng") = 1;
int bml_fake_map_server_rng __asm__("map_server_rng") = 1;
int bml_fake_multiplayer __asm__("multiplayer") = 1;
int bml_fake_clientnum __asm__("clientnum") = 1;
int bml_fake_openedChest __asm__("openedChest") = 1;
bool *bml_fake_shoparea __asm__("shoparea") = bml_fake_shoparea_storage;
