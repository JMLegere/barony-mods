/* Test-only ELF symbol provider for native hook smoke tests. */

int bml_fake_detour_counter __asm__("bml_fake_detour_counter") __attribute__((visibility("protected"))) = 0;

int bml_fake_detour_target(void);

__asm__(
    ".text\n"
    ".globl bml_fake_detour_target\n"
    ".type bml_fake_detour_target, @function\n"
    "bml_fake_detour_target:\n"
    "  xor %eax, %eax\n"
    "  test %eax, %eax\n"
    "  je 1f\n"
    "  mov $99, %eax\n"
    "1:\n"
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
    "  mov $42, %eax\n"
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

#define BML_FAKE_FUNCTION(c_name, elf_name) \
    void c_name(void) __asm__(elf_name); \
    void c_name(void) {}

BML_FAKE_FUNCTION(bml_fake_getChestInventoryList, "_ZN6Entity21getChestInventoryListEv")
BML_FAKE_FUNCTION(bml_fake_addItemToChest, "_ZN6Entity14addItemToChestEP4ItembS1_")
BML_FAKE_FUNCTION(bml_fake_getItemFromChest, "_ZN6Entity16getItemFromChestEP4Itemib")
BML_FAKE_FUNCTION(bml_fake_removeItemFromVoidChestServer, "_ZN6Entity29removeItemFromVoidChestServerEiP4Itemi")
BML_FAKE_FUNCTION(bml_fake_closeChest, "_ZN6Entity10closeChestEv")
BML_FAKE_FUNCTION(bml_fake_closeChestServer, "_ZN6Entity16closeChestServerEv")
BML_FAKE_FUNCTION(bml_fake_generateDungeon, "_Z15generateDungeonPcjSt5tupleIJiiiiEE")
BML_FAKE_FUNCTION(bml_fake_assignActions, "_Z13assignActionsP5map_t")
BML_FAKE_FUNCTION(bml_fake_setSpriteAttributes, "_Z19setSpriteAttributesP6EntityS0_S0_")
BML_FAKE_FUNCTION(bml_fake_newItem, "_Z7newItem8ItemType6StatusssjbP6list_t")
BML_FAKE_FUNCTION(bml_fake_list_FreeAll, "_Z12list_FreeAllP6list_t")
BML_FAKE_FUNCTION(bml_fake_list_RemoveNode, "_Z15list_RemoveNodeP6node_t")
BML_FAKE_FUNCTION(bml_fake_list_AddNodeLast, "_Z16list_AddNodeLastP6list_t")
BML_FAKE_FUNCTION(bml_fake_list_AddNodeFirst, "_Z17list_AddNodeFirstP6list_t")

int bml_fake_stats __asm__("stats") = 1;
int bml_fake_map __asm__("map") = 1;
int bml_fake_map_rng __asm__("map_rng") = 1;
int bml_fake_map_server_rng __asm__("map_server_rng") = 1;
int bml_fake_multiplayer __asm__("multiplayer") = 1;
int bml_fake_clientnum __asm__("clientnum") = 1;
int bml_fake_openedChest __asm__("openedChest") = 1;
int bml_fake_shoparea __asm__("shoparea") = 1;
int bml_fake_TileEntityList __asm__("TileEntityList") = 1;
