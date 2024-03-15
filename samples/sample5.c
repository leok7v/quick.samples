/* Copyright (c) Dmitry "Leo" Kuznetsov 2024 see LICENSE for details */
#include "quick.h"
#include "edit.h"

begin_c

static bool debug_layout; // = true;

const char* title = "Sample5";

static uic_edit_t edit0;
static uic_edit_t edit1;
static uic_edit_t edit2;
static uic_edit_t* edit[3] = { &edit0, &edit1, &edit2 };

static int32_t focused(void);
static void focus_back_to_edit(void);

uic_button(full_screen, "&Full Screen", 7.5, {
    app.full_screen(!app.is_full_screen);
});

uic_button(quit, "&Quit", 7.5, { app.close(); });

uic_button(fuzz, "Fu&zz", 7.5, {
    int32_t ix = focused();
    if (ix >= 0) {
        edit[ix]->fuzz(edit[ix]);
        fuzz->ui.pressed = edit[ix]->fuzzer != null;
        focus_back_to_edit();
    }
});

uic_checkbox(wb, "&Word Break", 7.5, {
    int32_t ix = focused();
    if (ix >= 0) {
        edit[ix]->wb = wb->ui.pressed;
//      traceln("edit[%d].wordbreak: %d", ix, edit[ix]->wb);
        focus_back_to_edit();
    }
});

uic_checkbox(ro, "&Read Only", 7.5, {
    int32_t ix = focused();
    if (ix >= 0) {
        edit[ix]->ro = ro->ui.pressed;
//      traceln("edit[%d].readonly: %d", ix, edit[ix]->ro);
        focus_back_to_edit();
    }
});

uic_checkbox(mono, "&Mono", 7.5, {
    int32_t ix = focused();
//  traceln("ix: %d %p %p %p", ix, app.focus, &edit0, &edit1);
    if (ix >= 0) {
        edit[ix]->set_font(edit[ix],
            mono->ui.pressed ? &app.fonts.mono : &app.fonts.regular);
        focus_back_to_edit();
    } else {
        mono->ui.pressed = !mono->ui.pressed;
    }
});

uic_checkbox(sl, "&Single Line", 7.5, {
    int32_t ix = focused();
    if (ix == 2) {
        sl->ui.pressed = true; // always single line
    } else if (0 <= ix && ix < 2) {
        uic_edit_t* e = edit[ix];
        e->sle = sl->ui.pressed;
//      traceln("edit[%d].multiline: %d", ix, e->multiline);
        if (e->sle) {
            e->select_all(e);
            e->paste(e, "Hello World! Single Line Edit", -1);
        }
        focus_back_to_edit();
    }
});

uic_multiline(text, 0.0, "...");

uic_container(right, null,
    &full_screen.ui, &quit.ui, &fuzz.ui,
    &wb.ui, &mono.ui, &sl.ui, &ro.ui, &edit2.ui);

uic_container(left, null, &edit0.ui, &edit1.ui);
uic_container(bottom, null, &text.ui);

static void set_text(int32_t ix) {
    sprintf(text.ui.text, "%d:%d %d:%d %dx%d\n"
        "scroll %03d:%03d",
        edit[ix]->selection[0].pn, edit[ix]->selection[0].gp,
        edit[ix]->selection[1].pn, edit[ix]->selection[1].gp,
        edit[ix]->ui.w, edit[ix]->ui.h,
        edit[ix]->scroll.pn, edit[ix]->scroll.rn);
    if (0) {
        traceln("%d:%d %d:%d %dx%d scroll %03d:%03d",
            edit[ix]->selection[0].pn, edit[ix]->selection[0].gp,
            edit[ix]->selection[1].pn, edit[ix]->selection[1].gp,
            edit[ix]->ui.w, edit[ix]->ui.h,
            edit[ix]->scroll.pn, edit[ix]->scroll.rn);
    }
    // can be called before text.ui initialized
    if (text.ui.invalidate != null) {
        text.ui.invalidate(&text.ui);
    }
}

static void after_paint(void) {
    // because of blinking caret paint is called frequently
    int32_t ix = focused();
    if (ix >= 0) {
        bool fuzzing = edit[ix]->fuzzer != null;
        if (fuzz.ui.pressed != fuzzing) {
            fuzz.ui.pressed = fuzzing;
            fuzz.ui.invalidate(&fuzz.ui);
        }
        set_text(ix);
    }
}

static void paint_frames(uic_t* ui) {
    for (uic_t** c = ui->children; c != null && *c != null; c++) {
        paint_frames(*c);
    }
    color_t fc[] = {
        colors.red, colors.green, colors.blue, colors.red,
        colors.yellow, colors.cyan, colors.magenta
    };
    static int32_t color;
    gdi.push(ui->x, ui->y + ui->h - ui->em.y);
    gdi.frame_with(ui->x, ui->y, ui->w, ui->h, fc[color]);
    color_t c = gdi.set_text_color(fc[color]);
    gdi.print("%s", ui->text);
    gdi.set_text_color(c);
    gdi.pop();
    color = (color + 1) % countof(fc);
}

static void null_paint(uic_t* ui) {
    for (uic_t** c = ui->children; c != null && *c != null; c++) {
        null_paint(*c);
    }
    if (ui != app.ui) {
        ui->paint = null;
    }
}

static void paint(uic_t* ui) {
    if (debug_layout) { null_paint(ui); }
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.black);
    gdi.fill(0, 0, ui->w, ui->h);
    int32_t ix = focused();
    for (int32_t i = 0; i < countof(edit); i++) {
        uic_t* e = &edit[i]->ui;
        color_t c = edit[i]->ro ?
            colors.tone_red : colors.btn_hover_highlight;
        gdi.frame_with(e->x - 1, e->y - 1, e->w + 2, e->h + 2,
            i == ix ? c : colors.dkgray4);
    }
    after_paint();
    if (debug_layout) { paint_frames(ui); }
    if (ix >= 0) {
        ro.ui.pressed = edit[ix]->ro;
        wb.ui.pressed = edit[ix]->wb;
        sl.ui.pressed = edit[ix]->sle;
        mono.ui.pressed = edit[ix]->ui.font == &app.fonts.mono;
    }
}

static void open_file(const char* pathname) {
    char* file = null;
    int64_t bytes = 0;
    if (crt.memmap_read(pathname, &file, &bytes) == 0) {
        if (0 < bytes && bytes <= INT_MAX) {
            edit[0]->select_all(edit[0]);
            edit[0]->paste(edit[0], file, (int32_t)bytes);
            uic_edit_pg_t start = { .pn = 0, .gp = 0 };
            edit[0]->move(edit[0], start);
        }
        crt.memunmap(file, bytes);
    } else {
        app.toast(5.3, "\nFailed to open file \"%s\".\n%s\n",
                  pathname, crt.error(crt.err()));
    }
}

static void opened(void) {
    if (app.argc > 1) {
        open_file(app.argv[1]);
    }
}

static int32_t focused(void) {
    // app.focus can point to a button, thus see which edit
    // control was focused last
    int32_t ix = -1;
    for (int32_t i = 0; i < countof(edit) && ix < 0; i++) {
        if (app.focus == &edit[i]->ui) { ix = i; }
        if (edit[i]->focused) { ix = i; }
    }
    static int32_t last_ix = -1;
    if (ix < 0) { ix = last_ix; }
    last_ix = ix;
    return ix;
}

static void focus_back_to_edit(void) {
    const int32_t ix = focused();
    if (ix >= 0) {
        app.focus = &edit[ix]->ui; // return focus where it was
    }
}

static void every_100ms(void) {
    static uic_t* last;
    if (last != app.focus) { app.redraw(); }
    last = app.focus;
}

static void measure(uic_t* ui) {
    // gaps:
    const int32_t gx = ui->em.x;
    const int32_t gy = ui->em.y;
    if (!edit[2]->sle) { // edit[2] is always SLE
        edit[2]->sle = true;
        edit[2]->select_all(edit[2]);
        edit[2]->paste(edit[2], "Single line edit control", -1);
    }
    right.h = ui->h - text.ui.h - gy;
    right.w = 0;
    measurements.vertical(&right, gy / 2);
    right.w += gx;
    bottom.w = text.ui.w - gx;
    bottom.h = text.ui.h;
    int32_t h = (ui->h - bottom.h - gy * 3) / countof(edit);
    for (int32_t i = 0; i < countof(edit); i++) {
        edit[i]->ui.w = ui->w - right.w - gx * 2;
        edit[i]->ui.h = h;
    }
    left.w = 0;
    measurements.vertical(&left, gy);
    left.w += gx;
    edit2.ui.h = ro.ui.h;
    edit2.ui.w = ro.ui.w;
    if (debug_layout) {
        traceln("%d,%d %dx%d", ui->x, ui->y, ui->w, ui->h);
        traceln("right %d,%d %dx%d", right.x, right.y, right.w, right.h);
        for (uic_t** c = right.children; c != null && *c != null; c++) {
            traceln("  %s %d,%d %dx%d", (*c)->text, (*c)->x, (*c)->y, (*c)->w, (*c)->h);
        }
        for (int32_t i = 0; i < countof(edit); i++) {
            traceln("[%d] %d,%d %dx%d", i, edit[i]->ui.x, edit[i]->ui.y,
                edit[i]->ui.w, edit[i]->ui.h);
        }
        traceln("left %d,%d %dx%d", left.x, left.y, left.w, left.h);
        traceln("bottom %d,%d %dx%d", bottom.x, bottom.y, bottom.w, bottom.h);
    }
}

static void layout(uic_t* ui) {
    // gaps:
    const int32_t gx2 = ui->em.x / 2;
    const int32_t gy2 = ui->em.y / 2;
    left.x = gx2;
    left.y = gy2;
    layouts.vertical(&left, left.x + gx2, left.y + gy2, gy2);
    right.x = left.x + left.w + gx2;
    right.y = left.y;
    bottom.x = gx2;
    bottom.y = ui->h - bottom.h;
    layouts.vertical(&right, right.x + gx2, right.y, gy2);
    text.ui.x = gx2;
    text.ui.y = ui->h - text.ui.h;
}

static void key_pressed(uic_t* unused(ui), int32_t key) {
    if (app.has_focus() && key == virtual_keys.escape) { app.close(); }
    int32_t ix = focused();
    if (key == virtual_keys.f5) {
        if (ix >= 0) {
            uic_edit_t* e = edit[ix];
            if (app.ctrl && app.shift && e->fuzzer == null) {
                e->fuzz(e); // start on Ctrl+Shift+F5
            } else if (e->fuzzer != null) {
                e->fuzz(e); // stop on F5
            }
        }
    }
    if (ix >= 0) { set_text(ix); }
}

static void edit_enter(uic_edit_t* e) {
    assert(e->sle);
    traceln("text: %.*s", e->para[0].bytes, e->para[0].text);
}

static void init(void) {
    app.title = title;
    app.ui->measure     = measure;
    app.ui->layout      = layout;
    app.ui->paint       = paint;
    app.ui->key_pressed = key_pressed;
    static uic_t* children[] = { &left, &right, &bottom, null };
    app.ui->children = children;
    text.ui.font = &app.fonts.mono;
    strprintf(fuzz.ui.tip, "Ctrl+Shift+F5 to start / F5 to stop Fuzzing");
    for (int32_t i = 0; i < countof(edit); i++) {
        uic_edit_init(edit[i]);
        edit[i]->enter = edit_enter;
    }
    app.focus = &edit[0]->ui;
    app.every_100ms = every_100ms;
    set_text(0); // need to be two lines for measure
}

app_t app = {
    .class_name = "sample5",
    .init   = init,
    .opened = opened,
    .wmin = 3.0f, // 3x2 inches
    .hmin = 2.0f
};

end_c
