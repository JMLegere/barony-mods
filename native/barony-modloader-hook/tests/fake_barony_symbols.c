/* Test-only ELF symbol provider for native hook smoke tests. */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

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

static BmlFakeStat bml_fake_stat_zero;
void *bml_fake_stats[1] __asm__("stats") = { &bml_fake_stat_zero };

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

void bml_fake_newEntity(void) __asm__("_Z9newEntityijP6list_tS0_");

__asm__(
    ".text\n"
    ".globl _Z9newEntityijP6list_tS0_\n"
    ".type _Z9newEntityijP6list_tS0_, @function\n"
    "_Z9newEntityijP6list_tS0_:\n"
    "  push %r14\n"
    "  mov %rdx, %r14\n"
    "  push %r13\n"
    "  mov %esi, %r13d\n"
    "  push %r12\n"
    "  push %rbp\n"
    "  mov %edi, %ebp\n"
    "  mov $56, %edi\n"
    "  pop %rbp\n"
    "  pop %r12\n"
    "  pop %r13\n"
    "  pop %r14\n"
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
    ".globl _ZN6Entity14addItemToChestEP4ItembS1_\n"
    ".type _ZN6Entity14addItemToChestEP4ItembS1_, @function\n"
    "_ZN6Entity14addItemToChestEP4ItembS1_:\n"
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
    "  call bml_fake_add_item_to_chest_impl@PLT\n"
    "  pop %rbp\n"
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

#define BML_FAKE_FUNCTION(c_name, elf_name) \
    void c_name(void) __asm__(elf_name); \
    void c_name(void) {}

BML_FAKE_FUNCTION(bml_fake_generateDungeon, "_Z15generateDungeonPcjSt5tupleIJiiiiEE")
BML_FAKE_FUNCTION(bml_fake_assignActions, "_Z13assignActionsP5map_t")
BML_FAKE_FUNCTION(bml_fake_setSpriteAttributes, "_Z19setSpriteAttributesP6EntityS0_S0_")

int bml_fake_map __asm__("map") = 1;
int bml_fake_map_rng __asm__("map_rng") = 1;
int bml_fake_map_server_rng __asm__("map_server_rng") = 1;
int bml_fake_multiplayer __asm__("multiplayer") = 1;
int bml_fake_clientnum __asm__("clientnum") = 1;
int bml_fake_openedChest __asm__("openedChest") = 1;
int bml_fake_shoparea __asm__("shoparea") = 1;
int bml_fake_TileEntityList __asm__("TileEntityList") = 1;
