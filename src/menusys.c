/*
  Copyright 2009, Ifcaro & volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/opl.h"
#include "include/menusys.h"
#include "include/supportbase.h"
#include "include/discsupport.h"
#include "include/iosupport.h"
#include "include/renderman.h"
#include "include/fntsys.h"
#include "include/lang.h"
#include "include/themes.h"
#include "include/pad.h"
#include "include/gui.h"
#include "include/guigame.h"
#include "include/system.h"
#include "include/ioman.h"
#include "include/sound.h"
#include "include/rabadge.h"
#include "include/caduceus.h"
#include "include/texcache.h"
#include "include/achievements.h"
#include <assert.h>

static image_cache_t *cardRAIconCache;
static int cardRAIconId = -1, cardRAIconUid;
static char cardRAIconHash[33], cardRAIconPrefix[64];
static item_list_t *cardRAIconSupport;
static int cardShowHash;
static int cardAction; /* 0: play, 1: back */
static int cardNeedsCheck;
static menu_item_t achievementCategory;
static int achievementCategoryInstalled;
static int achFromCard, achNeedsPage = 1, achPage, achFilter, achSelected, achLibraryPage;
static char achKind = 'G', achTarget[33] = "0";
static image_cache_t *achIcons;
static int achIconIds[ACH_PAGE_SIZE] = {-1, -1, -1}, achIconUids[ACH_PAGE_SIZE];
static void menuOpenAchievements(const char *hash);

enum MENU_IDs {
    MENU_SETTINGS = 0,
    MENU_GFX_SETTINGS,
    MENU_AUDIO_SETTINGS,
    MENU_CONTROLLER_SETTINGS,
    MENU_OSD_LANGUAGE_SETTINGS,
    MENU_PARENTAL_LOCK,
    MENU_NET_CONFIG,
    MENU_NET_UPDATE,
    MENU_START_NBD,
    MENU_ABOUT,
    MENU_SAVE_CHANGES,
    MENU_EXIT,
    MENU_POWER_OFF,
    MENU_RA_DISC_LAUNCH, /* RetroAchievements: boot the disc in the tray */
    MENU_RA_DISC_CHECK,  /* RetroAchievements: hash the disc and ask the PC */
};

enum GAME_MENU_IDs {
    GAME_COMPAT_SETTINGS = 0,
    GAME_CHEAT_SETTINGS,
    GAME_GSM_SETTINGS,
    GAME_VMC_SETTINGS,
#ifdef PADEMU
    GAME_PADEMU_SETTINGS,
    GAME_PADMACRO_SETTINGS,
#endif
    GAME_OSD_LANGUAGE_SETTINGS,
    GAME_SAVE_CHANGES,
    GAME_TEST_CHANGES,
    GAME_REMOVE_CHANGES,
    GAME_RENAME_GAME,
    GAME_DELETE_GAME,
    GAME_RA_CHECK, /* RetroAchievements: hash the image and ask the PC whether it is supported */
    GAME_RA_TEST,  /* RetroAchievements: check that the PC client is reachable */
};

// global menu variables
static menu_list_t *menu;
static menu_list_t *selected_item;

static int actionStatus;
static int itemConfigId;
static config_set_t *itemConfig;

static u8 parentalLockCheckEnabled = 1;

// "main menu submenu"
static submenu_list_t *mainMenu;
// active item in the main menu
static submenu_list_t *mainMenuCurrent;

// "game settings submenu"
static submenu_list_t *gameMenu;
// active item in game settings
static submenu_list_t *gameMenuCurrent;

static submenu_list_t *appMenu;
static submenu_list_t *appMenuCurrent;

static s32 menuSemaId;
static s32 menuListSemaId = -1;
static ee_sema_t menuSema;

static void menuRenameGame(submenu_list_t **submenu)
{
    if (!selected_item->item->current) {
        return;
    }

    if (!gEnableWrite)
        return;

    item_list_t *support = selected_item->item->userdata;

    if (support) {
        if (support->itemRename) {
            if (menuCheckParentalLock() == 0) {
                sfxPlay(SFX_MESSAGE);
                int nameLength = support->itemGetNameLength(support, selected_item->item->current->item.id);
                char newName[nameLength];
                strncpy(newName, selected_item->item->current->item.text, nameLength);
                if (guiShowKeyboard(newName, nameLength)) {
                    guiSwitchScreen(GUI_SCREEN_MAIN);
                    submenuDestroy(submenu);

                    // Only rename the file if the name changed; trying to rename a file with a file name that hasn't changed can cause the file
                    // to be deleted on certain file systems.
                    if (strcmp(newName, selected_item->item->current->item.text) != 0) {
                        support->itemRename(support, selected_item->item->current->item.id, newName);
                        ioPutRequest(IO_MENU_UPDATE_DEFFERED, &support->mode);
                    }
                }
            }
        }
    } else
        guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void menuDeleteGame(submenu_list_t **submenu)
{
    if (!selected_item->item->current)
        return;

    if (!gEnableWrite)
        return;

    item_list_t *support = selected_item->item->userdata;

    if (support) {
        if (support->itemDelete) {
            if (menuCheckParentalLock() == 0) {
                if (guiMsgBox(_l(_STR_DELETE_WARNING), 1, NULL)) {
                    guiSwitchScreen(GUI_SCREEN_MAIN);
                    submenuDestroy(submenu);
                    support->itemDelete(support, selected_item->item->current->item.id);
                    ioPutRequest(IO_MENU_UPDATE_DEFFERED, &support->mode);
                }
            }
        }
    } else
        guiMsgBox("NULL Support object. Please report", 0, NULL);
}

static void _menuLoadConfig()
{
    WaitSema(menuSemaId);
    if (!itemConfig) {
        item_list_t *list = selected_item->item->userdata;
        itemConfig = list->itemGetConfig(list, itemConfigId);
    }
    actionStatus = 0;
    SignalSema(menuSemaId);
}

static void _menuSaveConfig()
{
    int result;

    WaitSema(menuSemaId);
    result = configWrite(itemConfig);
    itemConfigId = -1; // to invalidate cache and force reload
    actionStatus = 0;
    SignalSema(menuSemaId);

    if (!result)
        setErrorMessage(_STR_ERROR_SAVING_SETTINGS);
}

static void _menuRequestConfig()
{
    WaitSema(menuSemaId);
    if (selected_item->item->current != NULL && itemConfigId != selected_item->item->current->item.id) {
        if (itemConfig) {
            configFree(itemConfig);
            itemConfig = NULL;
        }
        item_list_t *list = selected_item->item->userdata;
        if (itemConfigId == -1 || guiInactiveFrames >= list->delay) {
            itemConfigId = selected_item->item->current->item.id;
            ioPutRequest(IO_CUSTOM_SIMPLEACTION, &_menuLoadConfig);
        }
    } else if (itemConfig)
        actionStatus = 0;

    SignalSema(menuSemaId);
}

config_set_t *menuLoadConfig()
{
    actionStatus = 1;
    itemConfigId = -1;
    guiHandleDeferedIO(&actionStatus, _l(_STR_LOADING_SETTINGS), IO_CUSTOM_SIMPLEACTION, &_menuRequestConfig);
    return itemConfig;
}

// we don't want a pop up when transitioning to or refreshing Game Menu gui.
config_set_t *gameMenuLoadConfig(struct UIItem *ui)
{
    actionStatus = 1;
    itemConfigId = -1;
    guiGameHandleDeferedIO(&actionStatus, ui, IO_CUSTOM_SIMPLEACTION, &_menuRequestConfig);
    return itemConfig;
}

void menuSaveConfig()
{
    actionStatus = 1;
    guiHandleDeferedIO(&actionStatus, _l(_STR_SAVING_SETTINGS), IO_CUSTOM_SIMPLEACTION, &_menuSaveConfig);
}

static void menuInitMainMenu(void)
{
    if (mainMenu)
        submenuDestroy(&mainMenu);

    // initialize the menu
    submenuAppendItem(&mainMenu, -1, "RA: launch disc", MENU_RA_DISC_LAUNCH, -1);
    submenuAppendItem(&mainMenu, -1, "RA: check disc support", MENU_RA_DISC_CHECK, -1);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_SETTINGS, _STR_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_GFX_SETTINGS, _STR_GFX_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_AUDIO_SETTINGS, _STR_AUDIO_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_CONTROLLER_SETTINGS, _STR_CONTROLLER_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_OSD_LANGUAGE_SETTINGS, _STR_OSD_SETTINGS);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_PARENTAL_LOCK, _STR_PARENLOCKCONFIG);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_NET_CONFIG, _STR_NETCONFIG);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_NET_UPDATE, _STR_NET_UPDATE);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_START_NBD, _STR_STARTNBD);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_ABOUT, _STR_ABOUT);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_SAVE_CHANGES, _STR_SAVE_CHANGES);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_EXIT, _STR_EXIT);
    submenuAppendItem(&mainMenu, -1, NULL, MENU_POWER_OFF, _STR_POWEROFF);

    mainMenuCurrent = mainMenu;
    while (mainMenuCurrent && mainMenuCurrent->item.id != MENU_SETTINGS)
        mainMenuCurrent = mainMenuCurrent->next;
}

void menuReinitMainMenu(void)
{
    menuInitMainMenu();
}

void menuInitGameMenu(void)
{
    if (gameMenu)
        submenuDestroy(&gameMenu);

    // initialize the menu
    submenuAppendItem(&gameMenu, -1, "Detalhes / Caduceus", GAME_RA_CHECK, -1);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_COMPAT_SETTINGS, _STR_COMPAT_SETTINGS);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_CHEAT_SETTINGS, _STR_CHEAT_SETTINGS);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_GSM_SETTINGS, _STR_GSCONFIG);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_VMC_SETTINGS, _STR_VMC_SCREEN);
#ifdef PADEMU
    submenuAppendItem(&gameMenu, -1, NULL, GAME_PADEMU_SETTINGS, _STR_PADEMUCONFIG);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_PADMACRO_SETTINGS, _STR_PADMACROCONFIG);
#endif
    submenuAppendItem(&gameMenu, -1, NULL, GAME_OSD_LANGUAGE_SETTINGS, _STR_OSD_SETTINGS);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_SAVE_CHANGES, _STR_SAVE_CHANGES);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_TEST_CHANGES, _STR_TEST);
    submenuAppendItem(&gameMenu, -1, NULL, GAME_REMOVE_CHANGES, _STR_REMOVE_ALL_SETTINGS);
    if (gEnableWrite) {
        submenuAppendItem(&gameMenu, -1, NULL, GAME_RENAME_GAME, _STR_RENAME);
        submenuAppendItem(&gameMenu, -1, NULL, GAME_DELETE_GAME, _STR_DELETE);
    }

    gameMenuCurrent = gameMenu;
}

void menuInitAppMenu(void)
{
    if (appMenu)
        submenuDestroy(&appMenu);

    // initialize the menu
    submenuAppendItem(&appMenu, -1, NULL, 0, _STR_RENAME);
    submenuAppendItem(&appMenu, -1, NULL, 1, _STR_DELETE);

    appMenuCurrent = appMenu;
}

// -------------------------------------------------------------------------------------------
// ---------------------------------------- Menu manipulation --------------------------------
// -------------------------------------------------------------------------------------------
void menuInit()
{
    menu = NULL;
    selected_item = NULL;
    itemConfigId = -1;
    itemConfig = NULL;
    mainMenu = NULL;
    mainMenuCurrent = NULL;
    gameMenu = NULL;
    gameMenuCurrent = NULL;
    appMenu = NULL;
    appMenuCurrent = NULL;
    menuInitMainMenu();

    menuSema.init_count = 1;
    menuSema.max_count = 1;
    menuSema.option = 0;
    menuSemaId = CreateSema(&menuSema);
    if (menuListSemaId < 0) {
        menuListSemaId = sbCreateSemaphore();
    }
}

void menuEnd()
{
    if (achIcons) { cacheDestroyCache(achIcons); achIcons = NULL; }
    achievementCategoryInstalled = 0;
    if (cardRAIconCache) {
        cacheDestroyCache(cardRAIconCache);
        cardRAIconCache = NULL;
    }
    // destroy menu
    menu_list_t *cur = menu;

    while (cur) {
        menu_list_t *td = cur;
        cur = cur->next;

        if (td->item)
            submenuDestroy(&(td->item->submenu));

        menuRemoveHints(td->item);

        free(td);
    }

    submenuDestroy(&mainMenu);
    submenuDestroy(&gameMenu);
    submenuDestroy(&appMenu);

    if (itemConfig) {
        configFree(itemConfig);
        itemConfig = NULL;
    }

    DeleteSema(menuSemaId);
    DeleteSema(menuListSemaId);
    menuListSemaId = -1;
}

static menu_list_t *AllocMenuItem(menu_item_t *item)
{
    menu_list_t *it;

    it = malloc(sizeof(menu_list_t));

    it->prev = NULL;
    it->next = NULL;
    it->item = item;

    return it;
}

void menuAppendItem(menu_item_t *item)
{
    assert(item);

    WaitSema(menuListSemaId);

    if (menu == NULL) {
        menu = AllocMenuItem(item);
        selected_item = menu;
    } else {
        menu_list_t *cur = menu;

        // traverse till the end
        while (cur->next)
            cur = cur->next;

        // create new item
        menu_list_t *newitem = AllocMenuItem(item);

        // link
        cur->next = newitem;
        newitem->prev = cur;
    }

    SignalSema(menuListSemaId);
}

void menuInstallAchievementsCategory(void)
{
    if (achievementCategoryInstalled) return;
    memset(&achievementCategory, 0, sizeof(achievementCategory));
    achievementCategory.text = "Conquistas";
    achievementCategory.text_id = -1;
    achievementCategory.visible = 1;
    submenuAppendItem(&achievementCategory.submenu, -1, "Visualizar conquistas", 0, -1);
    achievementCategory.current = achievementCategory.pagestart = achievementCategory.submenu;
    achievementCategoryInstalled = 1;
    struct gui_update_t *operation = guiOpCreate(GUI_OP_ADD_MENU);
    operation->menu.menu = &achievementCategory;
    guiDeferUpdate(operation);
}

static void refreshMenuPosition(void)
{
    // Find the first menu in the list that is visible and set it as the active menu.
    if (menu == NULL)
        return;

    menu_list_t *cur = menu;
    while (cur->item->visible == 0 && cur->next)
        cur = cur->next;

    if (cur->item->visible == 0) {
        // No visible menu was found, just set the current menu to the first one in the list.
        selected_item = menu;
    } else
        selected_item = cur;
}

void submenuRebuildCache(submenu_list_t *submenu)
{
    while (submenu) {
        if (submenu->item.cache_id)
            free(submenu->item.cache_id);
        if (submenu->item.cache_uid)
            free(submenu->item.cache_uid);

        int size = gTheme->gameCacheCount * sizeof(int);
        submenu->item.cache_id = malloc(size);
        memset(submenu->item.cache_id, -1, size);
        submenu->item.cache_uid = malloc(size);
        memset(submenu->item.cache_uid, -1, size);

        submenu = submenu->next;
    }
}

static submenu_list_t *submenuAllocItem(int icon_id, char *text, int id, int text_id)
{
    submenu_list_t *it = (submenu_list_t *)malloc(sizeof(submenu_list_t));

    it->prev = NULL;
    it->next = NULL;
    it->item.icon_id = icon_id;
    it->item.text = text;
    it->item.text_id = text_id;
    it->item.id = id;
    it->item.cache_id = NULL;
    it->item.cache_uid = NULL;
    submenuRebuildCache(it);

    return it;
}

submenu_list_t *submenuAppendItem(submenu_list_t **submenu, int icon_id, char *text, int id, int text_id)
{
    if (*submenu == NULL) {
        *submenu = submenuAllocItem(icon_id, text, id, text_id);
        return *submenu;
    }

    submenu_list_t *cur = *submenu;

    // traverse till the end
    while (cur->next)
        cur = cur->next;

    // create new item
    submenu_list_t *newitem = submenuAllocItem(icon_id, text, id, text_id);

    // link
    cur->next = newitem;
    newitem->prev = cur;

    return newitem;
}

static void submenuDestroyItem(submenu_list_t *submenu)
{
    free(submenu->item.cache_id);
    free(submenu->item.cache_uid);

    free(submenu);
}

void submenuRemoveItem(submenu_list_t **submenu, int id)
{
    submenu_list_t *cur = *submenu;
    submenu_list_t *prev = NULL;

    while (cur) {
        if (cur->item.id == id) {
            submenu_list_t *next = cur->next;

            if (prev)
                prev->next = cur->next;

            if (*submenu == cur)
                *submenu = next;

            submenuDestroyItem(cur);

            cur = next;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }
}

void submenuDestroy(submenu_list_t **submenu)
{
    // destroy sub menu
    submenu_list_t *cur = *submenu;

    while (cur) {
        submenu_list_t *td = cur;
        cur = cur->next;

        submenuDestroyItem(td);
    }

    *submenu = NULL;
}

void menuAddHint(menu_item_t *menu, int text_id, int icon_id)
{
    // allocate a new hint item
    menu_hint_item_t *hint = malloc(sizeof(menu_hint_item_t));

    hint->text_id = text_id;
    hint->icon_id = icon_id;
    hint->next = NULL;

    if (menu->hints) {
        menu_hint_item_t *top = menu->hints;

        // rewind to end
        for (; top->next; top = top->next)
            ;

        top->next = hint;
    } else {
        menu->hints = hint;
    }
}

void menuRemoveHints(menu_item_t *menu)
{
    while (menu->hints) {
        menu_hint_item_t *hint = menu->hints;
        menu->hints = hint->next;
        free(hint);
    }
}

char *menuItemGetText(menu_item_t *it)
{
    if (it->text_id >= 0)
        return _l(it->text_id);
    else
        return it->text;
}

char *submenuItemGetText(submenu_item_t *it)
{
    if (it->text_id >= 0)
        return _l(it->text_id);
    else
        return it->text;
}

static void swap(submenu_list_t *a, submenu_list_t *b)
{
    submenu_list_t *pa, *nb;
    pa = a->prev;
    nb = b->next;

    a->next = nb;
    b->prev = pa;
    b->next = a;
    a->prev = b;

    if (pa)
        pa->next = b;

    if (nb)
        nb->prev = a;
}

// Sorts the given submenu by comparing the on-screen titles
void submenuSort(submenu_list_t **submenu)
{
    // a simple bubblesort
    // *submenu = mergeSort(*submenu);
    submenu_list_t *head;
    int sorted = 0;

    if ((submenu == NULL) || (*submenu == NULL) || ((*submenu)->next == NULL))
        return;

    head = *submenu;

    while (!sorted) {
        sorted = 1;

        submenu_list_t *tip = head;

        while (tip->next) {
            submenu_list_t *nxt = tip->next;

            char *txt1 = submenuItemGetText(&tip->item);
            char *txt2 = submenuItemGetText(&nxt->item);

            int cmp = strcasecmp(txt1, txt2);

            if (cmp > 0) {
                swap(tip, nxt);

                if (tip == head)
                    head = nxt;

                sorted = 0;
            } else {
                tip = tip->next;
            }
        }
    }

    *submenu = head;
}

static void menuNextH()
{
    struct menu_list *next = selected_item->next;
    while (next != NULL && next->item->visible == 0)
        next = next->next;

    // If we found a valid menu transition to it.
    if (next != NULL) {
        selected_item = next;
        itemConfigId = -1;
        sfxPlay(SFX_CURSOR);
    }
}

static void menuPrevH()
{
    struct menu_list *prev = selected_item->prev;
    while (prev != NULL && prev->item->visible == 0)
        prev = prev->prev;

    if (prev != NULL) {
        selected_item = prev;
        itemConfigId = -1;
        sfxPlay(SFX_CURSOR);
    } else {
        menuInitMainMenu();
        guiSwitchScreen(GUI_SCREEN_MENU);
        sfxPlay(SFX_CURSOR);
    }
}

static void menuFirstPage()
{
    submenu_list_t *cur = selected_item->item->current;
    if (cur) {
        if (cur->prev) {
            sfxPlay(SFX_CURSOR);
        }

        selected_item->item->current = selected_item->item->submenu;
        selected_item->item->pagestart = selected_item->item->current;
    }
}

static void menuLastPage()
{
    submenu_list_t *cur = selected_item->item->current;
    if (cur) {
        if (cur->next) {
            sfxPlay(SFX_CURSOR);
        }
        while (cur->next)
            cur = cur->next; // go to end

        selected_item->item->current = cur;

        int itms = ((items_list_t *)gTheme->itemsList->extended)->displayedItems;
        while (--itms && cur->prev) // and move back to have a full page
            cur = cur->prev;

        selected_item->item->pagestart = cur;
    }
}

static void menuNextV()
{
    submenu_list_t *cur = selected_item->item->current;

    if (cur && cur->next) {
        selected_item->item->current = cur->next;
        sfxPlay(SFX_CURSOR);

        // if the current item is beyond the page start, move the page start one page down
        cur = selected_item->item->pagestart;
        int itms = ((items_list_t *)gTheme->itemsList->extended)->displayedItems + 1;
        while (--itms && cur)
            if (selected_item->item->current == cur)
                return;
            else
                cur = cur->next;

        selected_item->item->pagestart = selected_item->item->current;
    } else { // wrap to start
        menuFirstPage();
    }
}

static void menuPrevV()
{
    submenu_list_t *cur = selected_item->item->current;

    if (cur && cur->prev) {
        selected_item->item->current = cur->prev;
        sfxPlay(SFX_CURSOR);

        // if the current item is on the page start, move the page start one page up
        if (selected_item->item->pagestart == cur) {
            int itms = ((items_list_t *)gTheme->itemsList->extended)->displayedItems + 1; // +1 because the selection will move as well
            while (--itms && selected_item->item->pagestart->prev)
                selected_item->item->pagestart = selected_item->item->pagestart->prev;
        }
    } else { // wrap to end
        menuLastPage();
    }
}

static void menuNextPage()
{
    submenu_list_t *cur = selected_item->item->pagestart;

    if (cur && cur->next) {
        int itms = ((items_list_t *)gTheme->itemsList->extended)->displayedItems + 1;
        sfxPlay(SFX_CURSOR);

        while (--itms && cur->next)
            cur = cur->next;

        selected_item->item->current = cur;
        selected_item->item->pagestart = selected_item->item->current;
    } else { // wrap to start
        menuFirstPage();
    }
}

static void menuPrevPage()
{
    submenu_list_t *cur = selected_item->item->pagestart;

    if (cur && cur->prev) {
        int itms = ((items_list_t *)gTheme->itemsList->extended)->displayedItems + 1;
        sfxPlay(SFX_CURSOR);

        while (--itms && cur->prev)
            cur = cur->prev;

        selected_item->item->current = cur;
        selected_item->item->pagestart = selected_item->item->current;
    } else { // wrap to end
        menuLastPage();
    }
}

void menuSetSelectedItem(menu_item_t *item)
{
    menu_list_t *itm = menu;

    while (itm) {
        if (itm->item == item) {
            selected_item = itm;
            return;
        }

        itm = itm->next;
    }
}

static void xmbDrawActions(submenu_list_t *first, submenu_list_t *current, const char *subtitle);

void menuRenderMenu()
{
    if (!mainMenuCurrent)
        mainMenuCurrent = mainMenu;
    xmbDrawActions(mainMenu, mainMenuCurrent, NULL);
}

int menuSetParentalLockCheckState(int enabled)
{
    int wasEnabled;

    wasEnabled = parentalLockCheckEnabled;
    parentalLockCheckEnabled = enabled ? 1 : 0;

    return wasEnabled;
}

int menuCheckParentalLock(void)
{
    const char *parentalLockPassword;
    char password[CONFIG_KEY_VALUE_LEN];
    int result;

    result = 0; // Default to unlocked.
    if (parentalLockCheckEnabled) {
        config_set_t *configOPL = configGetByType(CONFIG_OPL);

        // Prompt for password, only if one was set.
        if (configGetStr(configOPL, CONFIG_OPL_PARENTAL_LOCK_PWD, &parentalLockPassword) && (parentalLockPassword[0] != '\0')) {
            password[0] = '\0';
            if (diaShowKeyb(password, CONFIG_KEY_VALUE_LEN, 1, _l(_STR_PARENLOCK_ENTER_PASSWORD_TITLE))) {
                if (strncmp(parentalLockPassword, password, CONFIG_KEY_VALUE_LEN) == 0) {
                    result = 0;
                    parentalLockCheckEnabled = 0; // Stop asking for the password.
                } else if (strncmp(OPL_PARENTAL_LOCK_MASTER_PASS, password, CONFIG_KEY_VALUE_LEN) == 0) {
                    guiMsgBox(_l(_STR_PARENLOCK_DISABLE_WARNING), 0, NULL);

                    configRemoveKey(configOPL, CONFIG_OPL_PARENTAL_LOCK_PWD);
                    saveConfig(CONFIG_OPL, 1);

                    result = 0;
                    parentalLockCheckEnabled = 0; // Stop asking for the password.
                } else {
                    guiMsgBox(_l(_STR_PARENLOCK_PASSWORD_INCORRECT), 0, NULL);
                    result = EACCES;
                }
            } else // User aborted.
                result = EACCES;
        }
    }

    return result;
}

void menuHandleInputMenu()
{
    if (!mainMenu)
        return;

    if (!mainMenuCurrent)
        mainMenuCurrent = mainMenu;

    if (getKey(KEY_UP)) {
        sfxPlay(SFX_CURSOR);
        if (mainMenuCurrent->prev)
            mainMenuCurrent = mainMenuCurrent->prev;
        else // rewind to the last item
            while (mainMenuCurrent->next)
                mainMenuCurrent = mainMenuCurrent->next;
    }

    if (getKey(KEY_DOWN)) {
        sfxPlay(SFX_CURSOR);
        if (mainMenuCurrent->next)
            mainMenuCurrent = mainMenuCurrent->next;
        else
            mainMenuCurrent = mainMenu;
    }

    if (getKeyOn(gSelectButton)) {
        // execute the item via looking at the id of it
        int id = mainMenuCurrent->item.id;

        sfxPlay(SFX_CONFIRM);

        if (id == MENU_RA_DISC_LAUNCH) {
            /* Does not return when the disc boots. */
            discLaunch();
        } else if (id == MENU_RA_DISC_CHECK) {
            if (discCheckSupportDeferred())
                guiShowRANotice("Checking the disc, this takes a few seconds...", NULL);
            else
                guiShowRANotice("A disc check is already running", NULL);
        } else if (id == MENU_SETTINGS) {
            if (menuCheckParentalLock() == 0)
                guiShowConfig();
        } else if (id == MENU_GFX_SETTINGS) {
            if (menuCheckParentalLock() == 0)
                guiShowUIConfig();
        } else if (id == MENU_AUDIO_SETTINGS) {
            if (menuCheckParentalLock() == 0)
                guiShowAudioConfig();
        } else if (id == MENU_CONTROLLER_SETTINGS) {
            if (menuCheckParentalLock() == 0)
                guiShowControllerConfig();
        } else if (id == MENU_OSD_LANGUAGE_SETTINGS) {
            if (menuCheckParentalLock() == 0)
                guiGameShowOSDLanguageConfig(1);
        } else if (id == MENU_PARENTAL_LOCK) {
            if (menuCheckParentalLock() == 0)
                guiShowParentalLockConfig();
        } else if (id == MENU_NET_CONFIG) {
            if (menuCheckParentalLock() == 0)
                guiShowNetConfig();
        } else if (id == MENU_NET_UPDATE) {
            if (menuCheckParentalLock() == 0)
                guiShowNetCompatUpdate();
        } else if (id == MENU_START_NBD) {
            if (menuCheckParentalLock() == 0)
                handleLwnbdSrv();
        } else if (id == MENU_ABOUT) {
            guiShowAbout();
        } else if (id == MENU_SAVE_CHANGES) {
            if (menuCheckParentalLock() == 0) {
                guiGameSaveOSDLanguageGlobalConfig(configGetByType(CONFIG_GAME));
#ifdef PADEMU
                guiGameSavePadEmuGlobalConfig(configGetByType(CONFIG_GAME));
                guiGameSavePadMacroGlobalConfig(configGetByType(CONFIG_GAME));
#endif
                saveConfig(CONFIG_OPL | CONFIG_NETWORK | CONFIG_GAME, 1);
                menuSetParentalLockCheckState(1); // Re-enable parental lock check.
            }
        } else if (id == MENU_EXIT) {
            if (guiMsgBox(_l(_STR_CONFIRMATION_EXIT), 1, NULL))
                sysExecExit();
        } else if (id == MENU_POWER_OFF) {
            if (guiMsgBox(_l(_STR_CONFIRMATION_POFF), 1, NULL))
                sysPowerOff();
        }

        // so the exit press wont propagate twice
        readPads();
    }

    if (getKeyOn(KEY_RIGHT) || getKeyOn(KEY_START) || getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
        // Check if there is anything to show the user, at all.
        if (gAPPStartMode || gETHStartMode || gBDMStartMode || gHDDStartMode) {
            if (getKeyOn(KEY_RIGHT)) {
                menu_list_t *first = menu;
                while (first && !first->item->visible)
                    first = first->next;
                if (first) {
                    selected_item = first;
                    itemConfigId = -1;
                }
            }
            guiSwitchScreen(GUI_SCREEN_MAIN);
            refreshMenuPosition();
        }
    }
}

static void menuRenderElements(theme_element_t *elem)
{
    // selected_item can't be NULL here as we only allow to switch to "Main" rendering when there is at least one device activated
    _menuRequestConfig();

    WaitSema(menuSemaId);

    while (elem) {
        if (elem->drawElem && elem->type != 4 && elem->type != 5)
            elem->drawElem(selected_item, selected_item->item->current, itemConfig, elem);

        elem = elem->next;
    }
    SignalSema(menuSemaId);
}

/* XMB uses an open canvas, white focus and cool translucent neighbours. */

/* Numeric values from OPL's ELEM_ATTRIBUTE_TYPE in themes.c.
   Kept local so themes.h does not need to be changed. */
#define CAD_ELEM_GAME_IMAGE 3
#define CAD_ELEM_MENU_ICON  6

static int caduceusCountItems(menu_item_t *item)
{
    int n = 0;
    submenu_list_t *p = item ? item->submenu : NULL;
    while (p) { n++; p = p->next; }
    return n;
}

static int caduceusCurrentIndex(menu_item_t *item)
{
    int n = 0;
    submenu_list_t *p = item ? item->submenu : NULL;
    while (p) {
        if (p == item->current) return n;
        n++;
        p = p->next;
    }
    return 0;
}

static const char *caduceusCategoryName(menu_item_t *item)
{
    const char *s = item ? menuItemGetText(item) : NULL;
    return (s && s[0]) ? s : "BIBLIOTECA";
}



static void caduceusDrawVerticalArrow(int x, int y, int up)
{
    int tipY = up ? y + 2 : y + 14;
    int stemY = up ? y + 12 : y + 2;
    int wingY = up ? y + 7 : y + 9;

    rmDrawLine(x, tipY, x, stemY, CAD_MUTED);
    rmDrawLine(x, tipY, x - 4, wingY, CAD_MUTED);
    rmDrawLine(x, tipY, x + 4, wingY, CAD_MUTED);
}

/* Draw one existing OPL theme element at a temporary position/size.
   This lets Caduceus reuse the real menu icons and ART cache. */
static int caduceusDrawThemeType(theme_element_t *first, int type,
                                 menu_list_t *m, submenu_list_t *item,
                                 int x, int y, int w, int h,
                                 const char *cachePattern)
{
    theme_element_t *e = first;
    while (e) {
        if (e->type == type && e->drawElem) {
            mutable_image_t *image = (mutable_image_t *)e->extended;
            int ox=e->posX, oy=e->posY, ow=e->width, oh=e->height;
            short oa=e->aligned, os=e->scaled;
            image_texture_t *overlay = NULL;

            if (cachePattern && (!image || !image->cache ||
                !image->cache->suffix || strcmp(image->cache->suffix, cachePattern))) {
                e=e->next;
                continue;
            }

            e->posX=x; e->posY=y; e->width=w; e->height=h;
            e->aligned=ALIGN_NONE; e->scaled=SCALING_RATIO;
            if (cachePattern && !strcmp(cachePattern, "COV")) {
                overlay = image->overlayTexture;
                image->overlayTexture = NULL;
            }
            e->drawElem(m, item, itemConfig, e);
            if (cachePattern && !strcmp(cachePattern, "COV"))
                image->overlayTexture = overlay;

            e->posX=ox; e->posY=oy; e->width=ow; e->height=oh;
            e->aligned=oa; e->scaled=os;
            return 1;
        }
        e=e->next;
    }
    return 0;
}



/* A single rail position is shared by Games and Settings, so switching screens
   never clears the canvas or jumps the horizontal category positions. */
static float xmbRail = -1.0f;
static float xmbMove(float value, float target)
{
    float delta = target - value;
    if (delta > -0.01f && delta < 0.01f)
        return target;
    return value + delta * 0.24f;
}

static int xmbRailOffset(int target)
{
    if (xmbRail < 0.0f)
        xmbRail = target;
    xmbRail = xmbMove(xmbRail, target);
    return (int)(xmbRail * 92.0f);
}

static float xmbListPosition(submenu_list_t *list, int index)
{
    static submenu_list_t *previousList;
    static float position;
    if (list != previousList || index - position > 2.0f || position - index > 2.0f)
        position = index;
    previousList = list;
    position = xmbMove(position, index);
    return position;
}

static int xmbRowY(float relative)
{
    float extra = relative > 0.0f ? (relative < 1.0f ? relative : 1.0f) * 24.0f
                                 : (relative > -1.0f ? relative : -1.0f) * 14.0f;
    return (int)(234.0f + relative * 36.0f + extra);
}

static void xmbFocusText(int x, int y, const char *text, int selected)
{
    if (selected) {
        // A solid dark backing stays legible on bright user backgrounds and
        // avoids the coloured double edges of the glow on interlaced TVs.
        rmDrawRect(x - 6, y - 13, 375, 27, CAD_PANEL);
        rmDrawRect(x - 6, y - 13, 3, 27, CAD_ACCENT);
    }
    fntRenderString(gTheme->fonts[0], x, y, ALIGN_VCENTER, 365, 24, text, selected ? CAD_TEXT : CAD_MUTED);
}

/* Monochrome GS glyphs stay sharp at every video mode and need no VRAM atlas. */
static void xmbDeviceGlyph(int x, int y, int size, int mode, u64 color)
{
    int r = size / 2;
    if (mode == ETH_MODE) {
        rmDrawRect(x - 5, y - r, 10, 9, color);
        rmDrawLine(x, y - r + 9, x, y + 3, color);
        rmDrawLine(x - r + 4, y + 3, x + r - 4, y + 3, color);
        rmDrawLine(x - r + 4, y + 3, x - r + 4, y + r - 6, color);
        rmDrawLine(x + r - 4, y + 3, x + r - 4, y + r - 6, color);
        rmDrawRect(x - r, y + r - 8, 9, 8, color);
        rmDrawRect(x + r - 9, y + r - 8, 9, 8, color);
    } else if (mode == APP_MODE) {
        int w = r - 3;
        rmDrawRect(x - r, y - r, w, w, color);
        rmDrawRect(x + 3, y - r, w, w, color);
        rmDrawRect(x - r, y + 3, w, w, color);
        rmDrawRect(x + 3, y + 3, w, w, color);
    } else {
        rmDrawLine(x - r, y - r + 4, x + r, y - r + 4, color);
        rmDrawLine(x - r, y - r + 4, x - r, y + r - 4, color);
        rmDrawLine(x + r, y - r + 4, x + r, y + r - 4, color);
        rmDrawLine(x - r, y + r - 4, x + r, y + r - 4, color);
        rmDrawLine(x - r + 4, y + r - 10, x + r - 4, y + r - 10, color);
        rmDrawRect(x + r - 7, y + r - 8, 3, 2, color);
    }
}

static void caduceusDrawCategory(menu_list_t *entry, int x, int selected)
{
    if (entry->item == &achievementCategory) {
        u64 color = selected ? CAD_ACCENT : CAD_MUTED;
        rmDrawRect(x - 12, 96, 24, 21, color);
        rmDrawRect(x - 3, 117, 6, 12, color);
        rmDrawRect(x - 13, 129, 26, 3, color);
        rmDrawLine(x - 18, 99, x - 18, 113, color);
        rmDrawLine(x + 18, 99, x + 18, 113, color);
        rmDrawLine(x - 18, 113, x + 18, 113, color);
        if (selected) fntRenderString(gTheme->fonts[0], x, 147, ALIGN_CENTER, 180, 22, "Conquistas", CAD_TEXT);
        return;
    }
    int size = selected ? 42 : 30;
    item_list_t *support = entry->item->userdata;
    xmbDeviceGlyph(x, 112, size, support ? support->mode : APP_MODE, selected ? CAD_ACCENT : CAD_MUTED);
    if (selected)
        fntRenderString(gTheme->fonts[0], x, 147, ALIGN_CENTER, 140, 22,
                        caduceusCategoryName(entry->item), CAD_TEXT);
}



static void xmbDrawSettingsIcon(int x, int active)
{
    u64 color = active ? CAD_ACCENT : CAD_MUTED;
    rmDrawRect(x - 18, 103, 36, 20, color);
    rmDrawRect(x - 16, 101, 32, 24, color);
    rmDrawLine(x - 17, 112, x + 17, 112, GS_SETREG_RGBA(25, 55, 85, 90));
    rmDrawRect(x - 3, 110, 6, 5, color);
    rmDrawLine(x - 7, 102, x - 7, 95, color);
    rmDrawLine(x - 7, 95, x + 7, 95, color);
    rmDrawLine(x + 7, 95, x + 7, 102, color);
    if (active)
        fntRenderString(gTheme->fonts[0], x, 147, ALIGN_CENTER, 300, 24, _l(_STR_SETTINGS), CAD_TEXT);
}

static void caduceusRenderXMB(void)
{
    menu_list_t *it;
    submenu_list_t *game;
    int cat = 0, i = 0, index, count;
    char counter[32];
    guiDrawBGPlasma();
    if (!selected_item || !selected_item->item)
        return;
    theme_elems_t *elems = (selected_item->item->userdata &&
        ((item_list_t *)selected_item->item->userdata)->mode == APP_MODE)
        ? &gTheme->appsMainElems : &gTheme->mainElems;
    for (it = menu; it && it != selected_item; it = it->next)
        if (it->item->visible) cat++;
    int rail = xmbRailOffset(cat + 1);
    int contentX = 184 + (cat + 1) * 92 - rail;
    if (184 - rail > 24)
        xmbDrawSettingsIcon(184 - rail, 0);
    for (it = menu; it; it = it->next) {
        if (!it->item->visible) continue;
        int x = 184 + (++i) * 92 - rail;
        if (x > 24 && x < 616)
            caduceusDrawCategory(it, x, it == selected_item);
    }
    count = caduceusCountItems(selected_item->item);
    index = caduceusCurrentIndex(selected_item->item);
    snprintf(counter, sizeof(counter), "%d / %d", count ? index + 1 : 0, count);
    fntRenderString(gTheme->fonts[0], 600 - rmUnScaleX(fntCalcDimensions(gTheme->fonts[0], counter)), 32, ALIGN_NONE, 0, 0, counter, CAD_MUTED);
    if (!count)
        fntRenderString(gTheme->fonts[0], contentX + 51, 230, ALIGN_NONE, 365, 40, _l(_STR_NO_ITEMS), CAD_TEXT);
    float position = xmbListPosition(selected_item->item->submenu, index);
    i = 0;
    for (game = selected_item->item->submenu; game; game = game->next, i++) {
        float rel = i - position;
        int selected = i == index;
        if (rel < -1.2f) continue;
        if (rel > 4.0f) break;
        int y = xmbRowY(rel);
        int size = selected ? 48 : 22;
        int drawn = selected_item->item == &achievementCategory;
        if (drawn) {
            rmDrawRect(contentX - 10, y - 12, 20, 16, selected ? CAD_ACCENT : CAD_MUTED);
            rmDrawRect(contentX - 2, y + 4, 4, 8, selected ? CAD_ACCENT : CAD_MUTED);
            rmDrawRect(contentX - 10, y + 12, 20, 2, selected ? CAD_ACCENT : CAD_MUTED);
        } else drawn = caduceusDrawThemeType(elems->first, CAD_ELEM_GAME_IMAGE,
                            selected_item, game, contentX - size / 2, y - size / 2,
                            size, selected ? 64 : size, selected ? "COV" : "ICO");
        if (!drawn) {
            GSTEXTURE *fallback = thmGetTexture(DISC_DEFAULT);
            if (fallback && fallback->Mem)
                rmDrawPixmap(fallback, contentX - size / 2, y - size / 2, ALIGN_NONE, size, size, SCALING_RATIO, gDefaultCol);
        }
        xmbFocusText(contentX + 51, y, submenuItemGetText(&game->item), selected);
    }
    /* Keep device-specific actions and the configured confirm button accurate. */
    menu_hint_item_t *hint;
    int hintX = 32, hintY = 426;
    for (hint = selected_item->item->hints; hint; hint = hint->next) {
        if (hint->text_id != _STR_REFRESH)
            continue;
        GSTEXTURE *texture = thmGetTexture(hint->icon_id);
        int iconWidth = texture && texture->Height ? rmWideScale(texture->Width * 20 / texture->Height) + 2 : 0;
        int width = iconWidth + rmUnScaleX(fntCalcDimensions(gTheme->fonts[0], _l(hint->text_id)));
        if (hintX + width > 608) {
            hintX = 32;
            hintY += 24;
        }
        if (hintY > 450)
            break;
        hintX = guiDrawIconAndText(hint->icon_id, hint->text_id, gTheme->fonts[0], hintX, hintY, CAD_MUTED) + 18;
    }
}

void menuRenderMain(void)
{
    item_list_t *list;

    if (!selected_item || !selected_item->item)
        return;

    list = selected_item->item->userdata;
    if (selected_item->item == &achievementCategory) { caduceusRenderXMB(); return; }
    static item_list_t *enteredCategory;
    if (list != enteredCategory && !sbGameCheckBusy()) {
        moduleEnterCategory(list);
        enteredCategory = list;
    }

    /* Preserve the list metadata expected by paging/input code. */
    if (list && list->mode == APP_MODE)
        gTheme->itemsList = gTheme->appsItemsList;
    else
        gTheme->itemsList = gTheme->gamesItemsList;

    if (gTheme->itemsList && gTheme->itemsList->extended)
        ((items_list_t *)gTheme->itemsList->extended)->displayedItems = 6;

    /* Keep config loading in sync with the selected game, just like the
       original theme renderer did through menuRenderElements(). */
    _menuRequestConfig();

    caduceusRenderXMB();
}

void menuHandleInputMain()
{
    if (!selected_item || !selected_item->item)
        return;
    item_list_t *support = selected_item->item->userdata;
    if (selected_item->item == &achievementCategory) {
        if (getKeyOn(gSelectButton)) menuOpenAchievements(NULL);
        else if (getKey(KEY_LEFT)) menuPrevH();
        else if (getKey(KEY_RIGHT)) menuNextH();
        else if (getKeyOn(KEY_START)) { menuInitMainMenu(); guiSwitchScreen(GUI_SCREEN_MENU); }
        return;
    }
    if (getKeyOn(gSelectButton) && support && support->enabled &&
        support->mode != APP_MODE && selected_item->item->current) {
        DisableCron = 1;
        menuOpenGameCard();
        return;
    }
    if (getKey(KEY_LEFT)) {
        menuPrevH();
    } else if (getKey(KEY_RIGHT)) {
        menuNextH();
    } else if (getKey(KEY_UP)) {
        menuPrevV();
    } else if (getKey(KEY_DOWN)) {
        menuNextV();
    } else if (getKeyOn(KEY_CROSS)) {
        selected_item->item->execCross(selected_item->item);
    } else if (getKeyOn(KEY_TRIANGLE)) {
        selected_item->item->execTriangle(selected_item->item);
    } else if (getKeyOn(KEY_CIRCLE)) {
        selected_item->item->execCircle(selected_item->item);
    } else if (getKeyOn(KEY_SQUARE)) {
        selected_item->item->execSquare(selected_item->item);
    } else if (getKeyOn(KEY_START)) {
        // reinit main menu - show/hide items valid in the active context
        menuInitMainMenu();
        guiSwitchScreen(GUI_SCREEN_MENU);
    } else if (getKeyOn(KEY_SELECT)) {
        selected_item->item->refresh(selected_item->item);
    } else if (getKey(KEY_L1)) {
        menuPrevPage();
    } else if (getKey(KEY_R1)) {
        menuNextPage();
    } else if (getKeyOn(KEY_L2)) { // home
        menuFirstPage();
    } else if (getKeyOn(KEY_R2)) { // end
        menuLastPage();
    }

    // Last Played Auto Start
    if (RemainSecs < 0) {
        DisableCron = 1; // Disable Counter
        if (gSelectButton == KEY_CIRCLE)
            selected_item->item->execCircle(selected_item->item);
        else
            selected_item->item->execCross(selected_item->item);
    }
}


static base_game_info_t *menuCardImage(item_list_t *support)
{
    // HDD and apps use different item layouts: never cast them to an ISO record.
    if (!support || support->mode > ETH_MODE || !support->itemGet ||
        !selected_item || !selected_item->item->current)
        return NULL;
    base_game_info_t *game = support->itemGet(support, selected_item->item->current->item.id);
    if (game && !strcmp(game->startup, "SHARE"))
        return NULL;
    return game;
}

static void menuCheckSelectedRA(void)
{
    if (!selected_item || !selected_item->item->current)
        return;
    item_list_t *support = selected_item->item->userdata;
    base_game_info_t *game = menuCardImage(support);
    if (!game || !support->itemGetPrefix) {
        guiShowRANotice("Verificacao disponivel para imagens em USB/BDM e ETH.", NULL);
        return;
    }
    const char *prefix = support->itemGetPrefix(support);
    if (!sbGameCheckBusy())
        cardRAIconId = -1;
    if (!prefix || !sbHashGameDeferred(prefix, game->name, game->extension, game->startup, game->format))
        guiShowRANotice("Outra verificacao esta em andamento.", "Aguarde o resultado antes de tentar novamente.");
    else
        cardNeedsCheck = 0;
}

void menuOpenGameCard(void)
{
    cardShowHash = 0;
    cardAction = 0;
    cardNeedsCheck = 1;
    guiSwitchScreen(GUI_SCREEN_GAME_CARD);
    if (!selected_item || !selected_item->item->current)
        return;
    item_list_t *support = selected_item->item->userdata;
    base_game_info_t *game = menuCardImage(support);
    if (game && game->format != GAME_FORMAT_USBLD && support->itemGetPrefix && !sbGameCheckBusy()) {
        const char *prefix = support->itemGetPrefix(support);
        if (prefix)
            menuCheckSelectedRA();
    }
}

static void menuCardText(int x, int y, int width, int height, const char *text, u64 color)
{
    char wrapped[256];
    snprintf(wrapped, sizeof(wrapped), "%s", text ? text : "");
    fntFitString(gTheme->fonts[0], wrapped, width);
    fntRenderString(gTheme->fonts[0], x, y, ALIGN_NONE, width, height, wrapped, color);
}

static void menuCardAction(int x, int width, const char *text, int selected, int enabled)
{
    rmDrawRect(x + 2, 410, width, 42, CAD_SHADOW);
    rmDrawRect(x, 406, width, 42, selected ? CAD_ACCENT : CAD_BORDER);
    rmDrawRect(x + 1, 407, width - 2, 40, selected && enabled ? CAD_ACCENT : CAD_PANEL);
    fntRenderString(gTheme->fonts[0], x + width / 2, 419, ALIGN_CENTER, width - 12, 22,
                    text, !enabled ? CAD_MUTED : selected ? CAD_BG : CAD_TEXT);
}

static GSTEXTURE *menuCardRAIcon(item_list_t *support, const char *prefix, const char *hash)
{
    if (!support || !support->itemGetImage || !prefix || !hash[0])
        return NULL;
    if (!cardRAIconCache)
        cardRAIconCache = cacheInitCache(-1, "ART", 1, "RA", 1);
    if (support != cardRAIconSupport || strcmp(prefix, cardRAIconPrefix) || strcmp(hash, cardRAIconHash)) {
        cardRAIconId = -1;
        cardRAIconSupport = support;
        snprintf(cardRAIconHash, sizeof(cardRAIconHash), "%s", hash);
        snprintf(cardRAIconPrefix, sizeof(cardRAIconPrefix), "%s", prefix);
    }
    return cacheGetTexture(cardRAIconCache, support, &cardRAIconId, &cardRAIconUid, cardRAIconHash);
}

void menuRenderGameCard(void)
{
    guiDrawBGPlasma();
    if (!selected_item || !selected_item->item->current) {
        guiSwitchScreen(GUI_SCREEN_MAIN);
        return;
    }
    item_list_t *support = selected_item->item->userdata;
    if (!support || !support->itemGetName || !support->itemGetStartup)
        return;
    int id = selected_item->item->current->item.id;
    base_game_info_t *game = menuCardImage(support);
    const char *prefix = support->itemGetPrefix ? support->itemGetPrefix(support) : NULL;
    if (cardNeedsCheck && !sbGameCheckBusy()) {
        if (game && prefix && game->format != GAME_FORMAT_USBLD)
            menuCheckSelectedRA();
        else
            cardNeedsCheck = 0;
    }
    sb_ra_check_result_t check;
    memset(&check, 0, sizeof(check));
    if (game && prefix)
        sbGetGameCheck(prefix, game->name, game->extension, game->startup, game->format, &check);
    int saved = raBadgeHas(support, id);
    const char *status = saved ? "Lista RA salva neste dispositivo" : "ISO ainda nao verificada";
    const char *detail = "Voce pode jogar sem conquistas.";
    u64 statusColor = CAD_MUTED;
    switch (check.state) {
        case SB_RA_CHECKING:
            status = "Verificando a imagem...";
            detail = "Calculando hash e consultando o servidor. Aguarde.";
            statusColor = CAD_ACCENT;
            break;
        case SB_RA_SUPPORTED:
            status = "ISO compativel com conquistas";
            detail = check.detail;
            statusColor = CAD_ACCENT;
            break;
        case SB_RA_UNSUPPORTED:
            status = "Imagem nao reconhecida pelo RA";
            detail = check.detail[0] ? check.detail : "Nenhuma compatibilidade confirmada para esta ISO.";
            break;
        case SB_RA_ERROR:
            status = "Nao foi possivel verificar";
            detail = check.detail;
            break;
        case SB_RA_FORMAT_UNSUPPORTED:
            status = "Formato nao suportado pelo verificador";
            detail = check.detail;
            break;
    }
    if (!game) {
        status = "Verificacao de ISO indisponivel";
        detail = "Use a imagem ISO original em USB/BDM ou ETH.";
    }
    // Card shell: cover on the left, identity and live RA response on the right.
    rmDrawRect(30, 38, 592, 360, CAD_SHADOW);
    rmDrawRect(27, 34, 592, 360, CAD_SHADOW);
    rmDrawRect(24, 30, 592, 360, CAD_BORDER);
    rmDrawRect(25, 31, 590, 358, CAD_PANEL);
    rmDrawRect(36, 52, 176, 264, CAD_BG);
    int cover = caduceusDrawThemeType(gTheme->mainElems.first, CAD_ELEM_GAME_IMAGE,
                                    selected_item, selected_item->item->current,
                                    36, 52, 176, 264, "COV");
    if (!cover)
        menuCardText(70, 158, 125, 40, "PLAYSTATION 2", CAD_MUTED);
    menuCardText(36, 334, 178, 20, caduceusCategoryName(selected_item->item), CAD_MUTED);
    menuCardText(234, 48, 356, 20, "PLAYSTATION 2", CAD_ACCENT);
    menuCardText(234, 78, 356, 48, support->itemGetName(support, id), CAD_TEXT);
    rmDrawRect(233, 134, 365, 146, check.state == SB_RA_SUPPORTED ? CAD_ACCENT : CAD_BORDER);
    rmDrawRect(234, 135, 363, 144, CAD_BG);
    menuCardText(246, 144, 338, 20, "RETROACHIEVEMENTS", CAD_ACCENT);
    menuCardText(246, 172, 338, 40, status, statusColor);
    int detailY = 218;
    int detailX = 246, detailWidth = 338;
    if (check.state == SB_RA_SUPPORTED && check.title[0]) {
        GSTEXTURE *raIcon = menuCardRAIcon(support, prefix, check.hash);
        if (raIcon && raIcon->Mem) {
            rmDrawRect(245, 213, 50, 50, CAD_BORDER);
            rmDrawPixmap(raIcon, 246, 214, ALIGN_NONE, 48, 48, SCALING_RATIO, gDefaultCol);
            detailX = 306;
            detailWidth = 278;
        }
        menuCardText(detailX, 211, detailWidth, 20, check.title, CAD_TEXT);
        detailY = 236;
    }
    menuCardText(detailX, detailY, detailWidth, 38, detail, CAD_MUTED);
    if (check.hash[0])
        menuCardText(234, 285, 360, 20, cardShowHash ? check.hash : "L1: ver identificacao da imagem", CAD_MUTED);
    rmDrawLine(234, 312, 597, 312, CAD_BORDER);
    menuCardText(234, 322, 84, 20, "Game ID", CAD_MUTED);
    menuCardText(326, 322, 268, 20, support->itemGetStartup(support, id), CAD_TEXT);
    menuCardText(234, 346, 84, 20, "Arquivo", CAD_MUTED);
    char filename[192];
    if (game) {
        if (game->format == GAME_FORMAT_OLD_ISO)
            snprintf(filename, sizeof(filename), "%s.%s%s", game->startup, game->name, game->extension);
        else
            snprintf(filename, sizeof(filename), "%s%s", game->name, game->extension);
    } else {
        snprintf(filename, sizeof(filename), "%s", support->itemGetName(support, id));
    }
    menuCardText(326, 346, 268, 38, filename, CAD_TEXT);
    int pending = cardNeedsCheck || sbGameCheckBusy();
    menuCardAction(36, 180, pending ? "Verificando RA..." : "Jogar", cardAction == 0, !pending);
    menuCardAction(234, 216, "Conquistas", cardAction == 2, !pending && check.state == SB_RA_SUPPORTED);
    menuCardAction(466, 132, "Voltar", cardAction == 1, 1);
}

void menuHandleInputGameCard(void)
{
    if (!selected_item || !selected_item->item->current) {
        guiSwitchScreen(GUI_SCREEN_MAIN);
        return;
    }
    if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
        guiSwitchScreen(GUI_SCREEN_MAIN);
    } else if (getKeyOn(KEY_LEFT) || getKeyOn(KEY_RIGHT)) {
        static const int right[] = {2, 0, 1}, left[] = {1, 2, 0};
        cardAction = getKeyOn(KEY_RIGHT) ? right[cardAction] : left[cardAction];
        sfxPlay(SFX_CURSOR);
    } else if (getKeyOn(KEY_SELECT)) {
        menuCheckSelectedRA();
    } else if (getKeyOn(KEY_L1)) {
        cardShowHash = !cardShowHash;
    } else if (getKeyOn(KEY_SQUARE)) {
        selected_item->item->execSquare(selected_item->item);
    } else if (getKeyOn(gSelectButton)) {
        if (cardAction == 1) {
            guiSwitchScreen(GUI_SCREEN_MAIN);
            return;
        }
        if (cardNeedsCheck || sbGameCheckBusy())
            return;
        item_list_t *support = selected_item->item->userdata;
        base_game_info_t *game = menuCardImage(support);
        sb_ra_check_result_t check;
        if (cardAction == 2) {
            if (game && support->itemGetPrefix && sbGetGameCheck(support->itemGetPrefix(support),
                game->name, game->extension, game->startup, game->format, &check) && check.state == SB_RA_SUPPORTED)
                menuOpenAchievements(check.hash);
            return;
        }
        int ready = game && support->itemGetPrefix && sbGetGameCheck(support->itemGetPrefix(support),
                game->name, game->extension, game->startup, game->format, &check) &&
                check.state == SB_RA_SUPPORTED && check.session_ready;
        sbSetRALaunchEnabled(ready);
        if (gSelectButton == KEY_CIRCLE)
            selected_item->item->execCircle(selected_item->item);
        else
            selected_item->item->execCross(selected_item->item);
    }
}

static void menuOpenAchievements(const char *hash)
{
    achFromCard = hash != NULL;
    achKind = hash ? 'A' : 'G';
    snprintf(achTarget, sizeof(achTarget), "%s", hash ? hash : "0");
    achPage = achLibraryPage = achFilter = achSelected = 0;
    achNeedsPage = 1;
    guiSwitchScreen(GUI_SCREEN_ACHIEVEMENTS);
}

static void achievementReload(void)
{
    achNeedsPage = 1;
    achSelected = 0;
    for (int i = 0; i < ACH_PAGE_SIZE; i++) achIconIds[i] = -1;
}

static GSTEXTURE *achievementIcon(int row, const char *key)
{
    if (!key[0]) return NULL;
    item_list_t *provider = NULL;
    for (menu_list_t *it = menu; it; it = it->next) {
        item_list_t *support = it->item->userdata;
        if (support && support->mode == ETH_MODE && support->enabled) { provider = support; break; }
    }
    if (!provider) return NULL;
    if (!achIcons) achIcons = cacheInitCache(-1, "ART", 1, "RA", ACH_PAGE_SIZE);
    if (!achIcons) return NULL;
    return cacheGetTexture(achIcons, provider, &achIconIds[row], &achIconUids[row], (char *)key);
}

void menuRenderAchievements(void)
{
    achievement_page_t page;
    char text[160];
    static const char *filters[] = {"Todas", "Desbloqueadas", "Pendentes", "Hardcore"};
    guiDrawBGPlasma();
    if (achNeedsPage && !sbGameCheckBusy() && achievementsRequest(achKind, achPage, achFilter, achTarget)) {
        achNeedsPage = 0;
        for (int i = 0; i < ACH_PAGE_SIZE; i++) achIconIds[i] = -1;
    }
    achievementsSnapshot(&page);
    if (achNeedsPage) page.state = ACH_LOADING;
    menuCardText(36, 28, 380, 24, "CONQUISTAS", CAD_ACCENT);
    menuCardText(436, 28, 170, 24, page.user, CAD_MUTED);
    rmDrawLine(36, 59, 604, 59, CAD_BORDER);
    menuCardText(36, 74, 560, 42, achKind == 'G' ? "Minha biblioteca RetroAchievements" : page.title, CAD_TEXT);
    if (page.state == ACH_READY) {
        int pages = (page.total + ACH_PAGE_SIZE - 1) / ACH_PAGE_SIZE;
        snprintf(text, sizeof(text), "%d / %d", page.total ? page.page + 1 : 0, pages);
        menuCardText(518, 132, 90, 20, text, CAD_MUTED);
        if (achKind == 'A') {
            snprintf(text, sizeof(text), "%d / %d desbloqueadas  |  %s", page.earned, page.maximum, filters[achFilter]);
            menuCardText(36, 132, 470, 22, text, CAD_ACCENT);
            rmDrawRect(36, 120, 568, 3, CAD_BORDER);
            if (page.maximum) rmDrawRect(36, 120, 568 * page.earned / page.maximum, 3, CAD_ACCENT);
        } else {
            snprintf(text, sizeof(text), page.total == 1 ? "%d jogo na conta" : "%d jogos na conta", page.total);
            menuCardText(36, 132, 460, 22, text, CAD_MUTED);
        }
        if (achSelected >= page.count) achSelected = 0;
        for (int i = 0; i < page.count; i++) {
            achievement_entry_t *entry = &page.entries[i];
            int y = 164 + i * 65;
            rmDrawRect(36, y, 568, 59, i == achSelected ? CAD_BORDER : CAD_PANEL);
            if (i == achSelected) rmDrawRect(36, y, 3, 59, CAD_ACCENT);
            GSTEXTURE *icon = achievementIcon(i, entry->icon);
            if (icon && icon->Mem) rmDrawPixmap(icon, 45, y + 5, ALIGN_NONE, 48, 48, SCALING_RATIO, gDefaultCol);
            else {
                rmDrawRect(58, y + 13, 20, 17, CAD_MUTED);
                rmDrawRect(65, y + 30, 6, 10, CAD_MUTED);
                rmDrawRect(58, y + 40, 20, 3, CAD_MUTED);
            }
            menuCardText(108, y + 7, 480, 20, entry->title, i == achSelected ? CAD_TEXT : CAD_MUTED);
            if (achKind == 'G')
                snprintf(text, sizeof(text), "%d / %d conquistas  |  %d hardcore", entry->earned, entry->total, entry->hardcore);
            else
                snprintf(text, sizeof(text), "%d pts  |  %s", entry->points, entry->hardcore ? "Hardcore" : entry->earned ? "Desbloqueada" : "Pendente");
            menuCardText(108, y + 32, 480, 20, text, entry->earned ? CAD_ACCENT : CAD_MUTED);
        }
        if (page.count) {
            achievement_entry_t *entry = &page.entries[achSelected];
            menuCardText(36, 367, 560, 42, entry->description, CAD_TEXT);
            if (achKind == 'A' && entry->date[0]) menuCardText(36, 408, 560, 20, entry->date, CAD_MUTED);
        } else menuCardText(36, 195, 555, 60, achKind == 'G' ? "Nenhum jogo com progresso nesta conta." : "Nenhuma conquista neste filtro.", CAD_MUTED);
    } else {
        const char *message = page.state == ACH_LOADING ? "Carregando conquistas..." :
            page.state == ACH_OFFLINE ? "Conecte sua conta no Caduceus e ative o compartilhamento ETH para parear o console." :
            page.state == ACH_UNSUPPORTED ? "Este jogo nao possui conquistas no catalogo. Atualize o catalogo no Caduceus." :
            "Nao foi possivel consultar o Caduceus. Verifique a conexao e tente atualizar.";
        menuCardText(48, 188, 535, 110, message, CAD_TEXT);
        menuCardText(48, 315, 535, 40, "Voce pode voltar e continuar usando o OPL.", CAD_MUTED);
    }
    rmDrawLine(36, 433, 604, 433, CAD_BORDER);
    menuCardText(36, 442, 465, 22, achKind == 'G' ? "L1/R1: pagina   Confirmar: abrir" : "L1/R1: pagina   Quadrado: filtro", CAD_MUTED);
    GSTEXTURE *back = thmGetTexture(gSelectButton == KEY_CIRCLE ? CROSS_ICON : CIRCLE_ICON);
    if (back && back->Mem) rmDrawPixmap(back, 519, 442, ALIGN_NONE, 16, 16, SCALING_RATIO, gDefaultCol);
    menuCardText(542, 442, 65, 22, "Voltar", CAD_TEXT);
}

void menuHandleInputAchievements(void)
{
    achievement_page_t page;
    achievementsSnapshot(&page);
    int cancel = getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE);
    if (getKeyOn(KEY_START) && !achFromCard && achKind == 'G') {
        menuInitMainMenu(); guiSwitchScreen(GUI_SCREEN_MENU); return;
    }
    if (cancel) {
        if (achFromCard) guiSwitchScreen(GUI_SCREEN_GAME_CARD);
        else if (achKind == 'A') {
            achKind = 'G'; achPage = achLibraryPage; achFilter = 0;
            strcpy(achTarget, "0"); achievementReload();
        } else guiSwitchScreen(GUI_SCREEN_MAIN);
        return;
    }
    if (sbGameCheckBusy() || achNeedsPage) return;
    if (getKeyOn(KEY_SELECT)) { achievementReload(); return; }
    if (getKeyOn(KEY_SQUARE) && achKind == 'A') {
        achFilter = (achFilter + 1) % 4; achPage = 0; achievementReload(); return;
    }
    if (page.state != ACH_READY) return;
    if (getKeyOn(KEY_L1) && achPage > 0) { achPage--; achievementReload(); }
    else if (getKeyOn(KEY_R1) && (achPage + 1) * ACH_PAGE_SIZE < page.total) { achPage++; achievementReload(); }
    else if (getKey(KEY_UP) && page.count) { achSelected = (achSelected + page.count - 1) % page.count; sfxPlay(SFX_CURSOR); }
    else if (getKey(KEY_DOWN) && page.count) { achSelected = (achSelected + 1) % page.count; sfxPlay(SFX_CURSOR); }
    else if (getKeyOn(gSelectButton) && achKind == 'G' && page.count) {
        achLibraryPage = achPage;
        snprintf(achTarget, sizeof(achTarget), "%d", page.entries[achSelected].id);
        achKind = 'A'; achPage = achFilter = 0; achievementReload();
    }
}

void menuRenderInfo(void)
{
    guiDrawBGPlasma();
    item_list_t *list = selected_item->item->userdata;

    if (list->mode == APP_MODE) {
        menuRenderElements(gTheme->appsInfoElems.first);
        gTheme->itemsList = gTheme->appsItemsList;
    } else {
        menuRenderElements(gTheme->infoElems.first);
        gTheme->itemsList = gTheme->gamesItemsList;
    }
}

void menuHandleInputInfo()
{
    if (getKeyOn(KEY_CROSS)) {
        if (gSelectButton == KEY_CIRCLE)
            guiSwitchScreen(GUI_SCREEN_MAIN);
        else
            selected_item->item->execCross(selected_item->item);
    } else if (getKey(KEY_UP)) {
        menuPrevV();
    } else if (getKey(KEY_DOWN)) {
        menuNextV();
    } else if (getKeyOn(KEY_CIRCLE)) {
        if (gSelectButton == KEY_CROSS)
            guiSwitchScreen(GUI_SCREEN_MAIN);
        else
            selected_item->item->execCircle(selected_item->item);
    } else if (getKey(KEY_L1)) {
        menuPrevPage();
    } else if (getKey(KEY_R1)) {
        menuNextPage();
    } else if (getKeyOn(KEY_L2)) {
        menuFirstPage();
    } else if (getKeyOn(KEY_R2)) {
        menuLastPage();
    }
}

static void xmbDrawActions(submenu_list_t *first, submenu_list_t *current, const char *subtitle)
{
    submenu_list_t *it;
    int index = 0, i = 0;
    guiDrawBGPlasma();
    u64 icon = CAD_MUTED;
    int rail = xmbRailOffset(0);
    int contentX = 184 - rail;
    xmbDrawSettingsIcon(contentX, 1);
    int categoryX = 276 - rail;
    menu_list_t *category;
    for (category = menu; category && categoryX < 616; category = category->next) {
        if (!category->item->visible) continue;
        if (categoryX > 24)
            caduceusDrawCategory(category, categoryX, 0);
        categoryX += 92;
    }
    if (subtitle)
        fntRenderString(gTheme->fonts[0], contentX + 51, 258, ALIGN_NONE, 365, 20, subtitle, CAD_MUTED);
    for (it = first; it && it != current; it = it->next) index++;
    float position = xmbListPosition(first, index);
    for (it = first; it; it = it->next, i++) {
        float rel = i - position;
        int selected = i == index;
        if (rel < -1.2f) continue;
        if (rel > 4.0f) break;
        int y = xmbRowY(rel);
        int size = selected ? 12 : 7;
        // Compact outlined tiles replace the old diamond placeholders.
        u64 color = selected ? CAD_TEXT : icon;
        rmDrawLine(contentX - size, y - size, contentX + size, y - size, color);
        rmDrawLine(contentX + size, y - size, contentX + size, y + size, color);
        rmDrawLine(contentX + size, y + size, contentX - size, y + size, color);
        rmDrawLine(contentX - size, y + size, contentX - size, y - size, color);
        rmDrawLine(contentX - size + 4, y - 3, contentX + size - 4, y - 3, color);
        rmDrawLine(contentX - size + 4, y + 3, contentX + size - 4, y + 3, color);
        xmbFocusText(contentX + 51, y, submenuItemGetText(&it->item), selected);
    }
    if (current && current->prev) caduceusDrawVerticalArrow(contentX, 162, 1);
    guiDrawSubMenuHints();
}

void menuRenderGameMenu()
{
    if (!gameMenu || !selected_item || !selected_item->item->current) return;
    if (!selected_item->item->visible) {
        guiSwitchScreen(GUI_SCREEN_MAIN);
        return;
    }
    if (!gameMenuCurrent) gameMenuCurrent = gameMenu;
    xmbDrawActions(gameMenu, gameMenuCurrent,
                   submenuItemGetText(&selected_item->item->current->item));
}

void menuHandleInputGameMenu()
{
    if (!gameMenu)
        return;

    if (!gameMenuCurrent)
        gameMenuCurrent = gameMenu;

    if (getKey(KEY_UP)) {
        sfxPlay(SFX_CURSOR);
        if (gameMenuCurrent->prev)
            gameMenuCurrent = gameMenuCurrent->prev;
        else // rewind to the last item
            while (gameMenuCurrent->next)
                gameMenuCurrent = gameMenuCurrent->next;
    }

    if (getKey(KEY_DOWN)) {
        sfxPlay(SFX_CURSOR);
        if (gameMenuCurrent->next)
            gameMenuCurrent = gameMenuCurrent->next;
        else
            gameMenuCurrent = gameMenu;
    }

    if (getKeyOn(gSelectButton)) {
        // execute the item via looking at the id of it
        int menuID = gameMenuCurrent->item.id;

        sfxPlay(SFX_CONFIRM);

        if (menuID == GAME_RA_CHECK) {
            menuOpenGameCard();
        } else if (menuID == GAME_RA_TEST) {
            if (sbTestPCLinkDeferred())
                guiShowRANotice("Looking for the PC client...", NULL);
            else
                guiShowRANotice("A connection test is already running", NULL);
        } else if (menuID == GAME_COMPAT_SETTINGS) {
            guiGameShowCompatConfig(selected_item->item->current->item.id, selected_item->item->userdata, itemConfig);
        } else if (menuID == GAME_CHEAT_SETTINGS) {
            guiGameShowCheatConfig();
        } else if (menuID == GAME_GSM_SETTINGS) {
            guiGameShowGSConfig();
        } else if (menuID == GAME_VMC_SETTINGS) {
            guiGameShowVMCMenu(selected_item->item->current->item.id, selected_item->item->userdata);
#ifdef PADEMU
        } else if (menuID == GAME_PADEMU_SETTINGS) {
            guiGameShowPadEmuConfig(0);
        } else if (menuID == GAME_PADMACRO_SETTINGS) {
            guiGameShowPadMacroConfig(0);
#endif
        } else if (menuID == GAME_OSD_LANGUAGE_SETTINGS) {
            guiGameShowOSDLanguageConfig(0);
        } else if (menuID == GAME_SAVE_CHANGES) {
            if (guiGameSaveConfig(itemConfig, selected_item->item->userdata))
                configSetInt(itemConfig, CONFIG_ITEM_CONFIGSOURCE, CONFIG_SOURCE_USER);
            menuSaveConfig();
            saveConfig(CONFIG_GAME, 0);
            guiMsgBox(_l(_STR_GAME_SETTINGS_SAVED), 0, NULL);
            guiGameLoadConfig(selected_item->item->userdata, gameMenuLoadConfig(NULL));
        } else if (menuID == GAME_TEST_CHANGES) {
            guiGameTestSettings(selected_item->item->current->item.id, selected_item->item->userdata, itemConfig);
        } else if (menuID == GAME_REMOVE_CHANGES) {
            if (guiGameShowRemoveSettings(itemConfig, configGetByType(CONFIG_GAME))) {
                guiGameLoadConfig(selected_item->item->userdata, gameMenuLoadConfig(NULL));
            }
        } else if (menuID == GAME_RENAME_GAME) {
            menuRenameGame(&gameMenu);
        } else if (menuID == GAME_DELETE_GAME) {
            menuDeleteGame(&gameMenu);
        }
        // so the exit press wont propagate twice
        readPads();
    }

    if (getKeyOn(KEY_START) || getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
        guiSwitchScreen(GUI_SCREEN_MAIN);
    }
}

void menuRenderAppMenu()
{
    if (!appMenu || !selected_item || !selected_item->item->current) return;
    if (!appMenuCurrent) appMenuCurrent = appMenu;
    xmbDrawActions(appMenu, appMenuCurrent,
                   submenuItemGetText(&selected_item->item->current->item));
}

void menuHandleInputAppMenu()
{
    if (!appMenu)
        return;

    if (!appMenuCurrent)
        appMenuCurrent = appMenu;

    if (getKey(KEY_UP)) {
        sfxPlay(SFX_CURSOR);
        if (appMenuCurrent->prev)
            appMenuCurrent = appMenuCurrent->prev;
        else // rewind to the last item
            while (appMenuCurrent->next)
                appMenuCurrent = appMenuCurrent->next;
    }

    if (getKey(KEY_DOWN)) {
        sfxPlay(SFX_CURSOR);
        if (appMenuCurrent->next)
            appMenuCurrent = appMenuCurrent->next;
        else
            appMenuCurrent = appMenu;
    }

    if (getKeyOn(gSelectButton)) {
        // execute the item via looking at the id of it
        int menuID = appMenuCurrent->item.id;

        sfxPlay(SFX_CONFIRM);

        if (menuID == 0) {
            menuRenameGame(&appMenu);
        } else if (menuID == 1) {
            menuDeleteGame(&appMenu);
        }
        // so the exit press wont propagate twice
        readPads();
    }

    if (getKeyOn(KEY_START) || getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
        guiSwitchScreen(GUI_SCREEN_MAIN);
    }
}
