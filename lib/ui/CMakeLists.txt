SET(SOURCES ui_comp_mainbuttontemplate.c
    ui_comp.c
    ui_SplashScreen.c
    ui_HomeScreen.c
    ui_Settings.c
    ui_DevScreen.c
    ui_theme_manager.c
    ui_themes.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c)

add_library(ui ${SOURCES})
