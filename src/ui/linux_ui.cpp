
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <dlfcn.h>
#ifdef USE_GTK
#include <gtk/gtk.h>
#endif

#include "gl_lite.h"
#include <X11/Xlib.h>
#include <GL/glx.h>
#include "libs/imgui/imgui.h"
#include "libs/easytab.h"
#include "libs/easykey.h"
#include "ui.h"

#define GLEW_NO_GLU

#define PAPAYA_DEFAULT_IMAGE "/home/apoorvaj/Pictures/o1.png"


Display* xlib_display;
Window xlib_window;
// =================================================================================================

void platform::print(const char* msg)
{
    printf("%s", msg);
}

void platform::start_mouse_capture()
{
    //
}

void platform::stop_mouse_capture()
{
    //
}

void platform::set_mouse_position(i32 x, i32 y)
{
    XWarpPointer(xlib_display, None, xlib_window, 0, 0, 0, 0, x, y);
    XFlush(xlib_display);
}

void platform::set_cursor_visibility(bool visible)
{
    if (!visible) {
        // Make a blank cursor
        char empty[1] = {0};
        Pixmap blank = XCreateBitmapFromData (xlib_display, xlib_window, empty, 1, 1);
        if(blank == None) { fprintf(stderr, "error: out of memory.\n"); }
        XColor dummy;
        Cursor invis_cursor = XCreatePixmapCursor(xlib_display, blank, blank, &dummy, &dummy, 0, 0);
        XFreePixmap (xlib_display, blank);
        XDefineCursor(xlib_display, xlib_window, invis_cursor);
    } else {
        XUndefineCursor(xlib_display, xlib_window); // TODO: Test what happens if cursor is attempted to be shown when it is not hidden in the first place.
    }
}

char* platform::open_file_dialog()
{
#ifdef USE_GTK
    GtkWidget *dialog =
        gtk_file_chooser_dialog_new("Open File", NULL,
                                    GTK_FILE_CHOOSER_ACTION_OPEN, "Cancel",
                                    GTK_RESPONSE_CANCEL, "Open",
                                    GTK_RESPONSE_ACCEPT, NULL);
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.[pP][nN][gG]");
    gtk_file_filter_add_pattern(filter, "*.[jJ][pP][gG]");
    gtk_file_filter_add_pattern(filter, "*.[jJ][pP][eE][gG]");
    gtk_file_chooser_add_filter(chooser, filter);

    char *out_file_name = 0;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *gtk_file_name = gtk_file_chooser_get_filename(chooser);
        int file_name_len = strlen(gtk_file_name) + 1; // +1 for terminator
        out_file_name = (char*)malloc(file_name_len);
        strcpy(out_file_name, gtk_file_name);
        g_free(gtk_file_name);
    }

    gtk_widget_destroy(dialog);

    return out_file_name;
#else
    return 0;
#endif
}

char* platform::save_file_dialog()
{
#if USE_GTK
    GtkWidget *dialog =
        gtk_file_chooser_dialog_new("Save File", NULL,
                                    GTK_FILE_CHOOSER_ACTION_SAVE, "Cancel",
                                    GTK_RESPONSE_CANCEL, "Save",
                                    GTK_RESPONSE_ACCEPT, NULL);
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.[pP][nN][gG]");
    gtk_file_chooser_add_filter(chooser, filter);
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
    gtk_file_chooser_set_filename(chooser, "untitled.png"); // what if the user saves a file that already has a name?

    char *out_file_name = 0;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *gtk_file_name = gtk_file_chooser_get_filename(chooser);
        int file_name_len = strlen(gtk_file_name) + 1; // +1 for terminator
        out_file_name = (char*)malloc(file_name_len);
        strcpy(out_file_name, gtk_file_name);
        g_free(gtk_file_name);
    }

    gtk_widget_destroy(dialog);

    return out_file_name;
#else
    return 0;
#endif
}

// =================================================================================================

int main(int argc, char **argv)
{
    PapayaMemory* mem = (PapayaMemory*) calloc(1, sizeof(*mem));
    XVisualInfo* xlib_visual_info;
    Atom xlib_delete_window_atom;

    timer::init(); // TODO: Check linux timer manual. Is this correct?
    timer::start(Timer_Startup);

    // Initialize GTK for Open/Save file dialogs
#ifdef USE_GTK
    gtk_init(&argc, &argv);
#endif

    // Set path to executable path
    {
        char path_buf[PATH_MAX];
        ssize_t path_len = readlink("/proc/self/exe", path_buf, sizeof(path_buf)-1);
        if (path_len != -1) {
            char *last_slash = strrchr(path_buf, '/');
            if (last_slash != NULL) { *last_slash = '\0'; }
            chdir(path_buf);
        }
    }

    // Create window
    {
        // TODO: Break this scope into smaller, well-defined scopes
        xlib_display = XOpenDisplay(0);
        if (!xlib_display) {
            // TODO: Log: Error opening connection to the X server
            exit(1);
        }

        i32 attribs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
        xlib_visual_info = glXChooseVisual(xlib_display, 0, attribs);
        if(xlib_visual_info == NULL) {
            // TODO: Log: No appropriate visual found
            exit(1);
        }

        XSetWindowAttributes window_attribs = {0};
        window_attribs.colormap =
            XCreateColormap(xlib_display, DefaultRootWindow(xlib_display),
                            xlib_visual_info->visual, AllocNone);
        window_attribs.event_mask =
            ExposureMask | PointerMotionMask | ButtonPressMask |
            ButtonReleaseMask | KeyPressMask | KeyReleaseMask;

        xlib_window = XCreateWindow(xlib_display, DefaultRootWindow(xlib_display),
                                    0, 0, 1366, 768, 0, xlib_visual_info->depth,
                                    InputOutput, xlib_visual_info->visual,
                                    CWColormap | CWEventMask, &window_attribs);
        mem->window.width = 1366;
        mem->window.height = 768;

        XMapWindow(xlib_display, xlib_window);
        XStoreName(xlib_display, xlib_window, "Papaya");

        xlib_delete_window_atom = XInternAtom(xlib_display, "WM_DELETE_WINDOW",
                                              False);
        XSetWMProtocols(xlib_display, xlib_window, &xlib_delete_window_atom, 1);

        // Window icon
        {
            // TODO: Investigate the "proper" way to set an application icon in
            //       Linux. This current method seems a too hacky.
            /*
               Utility to print out icon data
               ==============================
               Linux apparently needs pixel data to be in a 64-bit-per-pixel buffer in ARGB format.
               stb_image gives a 32-bit-per-pixel ABGR output. This function is used to pre-calculate
               and print the swizzled and expanded buffer in the correct format to minimize startup time.

               TODO: The 48x48 icon is rendered blurry in the dock (at least on Elementary OS). Investigate why.
            */
            #if 0
            {
                i32 ImageWidth, ImageHeight, ComponentsPerPixel;
                u8* Image = stbi_load("../../img/icon48.png", &ImageWidth, &ImageHeight, &ComponentsPerPixel, 4);
                printf("{%d,%d",ImageWidth, ImageHeight);
                for (i32 i = 0; i < ImageWidth * ImageHeight; i++)
                {
                    u32 Pixel = ((u32*)Image)[i];
                    u32 ARGB = 	((Pixel & 0xff000000) >> 00) |
                                    ((Pixel & 0x000000ff) << 16) |
                                    ((Pixel & 0x0000ff00) >> 00) |
                                    ((Pixel & 0x00ff0000) >> 16);

                    printf(",%u", ARGB);
                }
                printf("};\n");
            }
            #endif

            u64 buffer[] = {48,48,0,0,0,0,0,0,0,0,0,0,0,2960687,0,0,2960687,19737903,0,422391087,1445801263,2402102575,3140300079,3710725423,4063046959,4247596335,4247596335,4063046959,3710725423,3140300079,2402102575,1445801263,422391087,0,19737903,2960687,0,0,2960687,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2960687,0,0,2960687,36515119,372059439,1714236719,3375181103,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3375181103,1714236719,372059439,36515119,2960687,0,0,2960687,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2960687,0,0,36515119,556608815,2267884847,4046269743,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4046269743,2267884847,556608815,36515119,0,0,2960687,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2960687,2960687,0,2960687,321727791,1848454447,3928829231,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3928829231,1848454447,321727791,2960687,0,2960687,2960687,0,0,0,0,0,0,0,0,0,0,0,2960687,0,0,19737903,892153135,3089968431,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3089968431,892153135,19737903,0,0,2960687,0,0,0,0,0,0,0,0,0,2960687,0,0,86846767,1445801263,3794611503,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3794611503,1445801263,86846767,0,0,2960687,0,0,0,0,0,0,0,2960687,0,0,137178415,1764568367,4113378607,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4113378607,1764568367,137178415,0,0,2960687,0,0,0,0,0,0,2960687,0,86846767,1764568367,4214041903,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4214041903,1764568367,86846767,0,2960687,0,0,0,0,0,2960687,0,19737903,1445801263,4113378607,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4113378607,1445801263,19737903,0,2960687,0,0,0,2960687,0,2960687,892153135,3794611503,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281084975,4280756526,4280756526,4281084975,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3794611503,892153135,2960687,0,2960687,0,0,0,0,321727791,3089968431,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4280822062,4281282096,4284436027,4284436027,4281282352,4280822062,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3089968431,321727791,0,0,0,2960687,0,36515119,1848454447,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4280887854,4280756526,4283516472,4287327302,4291401300,4291401300,4287327558,4283516472,4280756526,4280887854,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,1848454447,36515119,0,2960687,0,2960687,556608815,3928829231,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281019439,4280493613,4282530868,4286473283,4290284369,4293044058,4292846938,4292846938,4293044058,4290284369,4286473283,4282530868,4280493613,4281019439,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3928829231,556608815,2960687,0,0,36515119,2267884847,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281085231,4280624941,4281545009,4285487423,4289364557,4292518489,4292912730,4292649817,4292649817,4292649817,4292649817,4292912730,4292518489,4289364557,4285487423,4281545009,4280624941,4281085231,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,2267884847,36515119,0,2960687,372059439,4046269743,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4280822318,4280822318,4284304698,4288641611,4291795798,4292912730,4292715609,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292715609,4292912730,4291795798,4288641611,4284304698,4280822318,4280822318,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4046269743,372059439,2960687,19737903,1714236719,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281019182,4280493613,4282990646,4287721799,4291138387,4292781402,4292846938,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292846938,4292781402,4291138387,4287721799,4282990646,4280493613,4281019182,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,1714236719,19737903,0,3375181103,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4280953646,4282070834,4286538819,4290481489,4292452697,4292846938,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292846938,4292452697,4290481489,4286538818,4282070577,4280953646,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3375181103,0,422391087,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281084975,4281150767,4286867524,4292452696,4292846938,4292715609,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292715353,4292781145,4292584798,4287065420,4281151023,4281084975,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,422391087,1445801263,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281084975,4281216560,4288050249,4293306715,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649560,4292650331,4292979814,4293834866,4288380246,4281216560,4281084975,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,1445801263,2402102575,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281084975,4281216560,4287984456,4293175643,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649816,4292649817,4292913763,4293177709,4293243760,4293835124,4288314711,4281216560,4281084975,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,2402102575,3140300079,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281084975,4281216560,4287984456,4293175643,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292781919,4293111659,4293243760,4293243761,4293243761,4293835124,4288314711,4281216560,4281084975,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3140300079,3710725423,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281084975,4281216560,4287984456,4293175643,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292715867,4292980073,4293243761,4293243761,4293243761,4293243761,4293243761,4293835124,4288314711,4281216560,4281084975,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3710725423,4063046959,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281084975,4281216560,4287984456,4293175643,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649818,4292913764,4293243504,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293835124,4288314711,4281216560,4281084975,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4063046959,4247596335,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281084975,4281216560,4287984456,4293175643,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292847712,4293177710,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293835124,4288314711,4281216560,4281084975,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4247596335,4247596335,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281084975,4281216560,4287984456,4293175643,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293835124,4288314711,4281216560,4281084975,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4247596335,4063046959,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281084975,4281216560,4287984456,4293175643,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293835124,4288314711,4281216560,4281084975,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4063046959,3710725423,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281084975,4281216560,4287984456,4293175643,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293835124,4288314711,4281216560,4281084975,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3710725423,3140300079,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281084975,4281216560,4287984456,4293175643,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293835124,4288314711,4281216560,4281084975,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3140300079,2402102575,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281084975,4281216560,4287984456,4293175643,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293835124,4288314711,4281216560,4281084975,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,2402102575,1445801263,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281084975,4281216560,4288050249,4293306715,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293900917,4288446039,4281216560,4281084975,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,1445801263,422391087,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281084975,4281150767,4286933060,4292452696,4292846938,4292715609,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293309553,4293440882,4293046640,4287197264,4281150767,4281084975,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,422391087,0,3375181103,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4280953646,4282136370,4286538819,4290481489,4292452697,4292846938,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4292649817,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293243761,4293441138,4293046640,4291009125,4286803022,4282136628,4280953646,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3375181103,0,19737903,1714236719,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281019182,4280493613,4282990646,4287721799,4291138387,4292781402,4292846938,4292649817,4292649817,4292649817,4292649817,4292649817,4293243761,4293243761,4293243761,4293243761,4293243761,4293440882,4293440882,4291666280,4288117333,4283056697,4280493611,4280953646,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,1714236719,19737903,2960687,372059439,4046269743,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4280822318,4280822318,4284304698,4288641611,4291795798,4292912730,4292715609,4292649817,4292649817,4292649817,4293243761,4293243761,4293243761,4293309553,4293572211,4292389228,4289037402,4284437057,4280822061,4280822061,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4046269743,372059439,2960687,0,36515119,2267884847,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281085231,4280624941,4281545009,4285487423,4289430093,4292518489,4292912730,4292649817,4292649817,4293243761,4293243761,4293572211,4293112176,4289826143,4285751368,4281545265,4280559404,4281085231,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,2267884847,36515119,0,0,2960687,556608815,3928829231,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281019439,4280493613,4282530868,4286473283,4290284369,4293044058,4292846937,4293440882,4293638003,4290812004,4286737230,4282596663,4280493611,4281019438,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3928829231,556608815,2960687,0,2960687,0,36515119,1848454447,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4280887854,4280756526,4283516472,4287327558,4291401300,4291929450,4287591762,4283648317,4280756525,4280887854,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,1848454447,36515119,0,2960687,0,0,0,321727791,3089968431,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4280822062,4281282352,4284436027,4284568386,4281282352,4280756525,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3089968431,321727791,0,0,0,0,2960687,0,2960687,892153135,3794611503,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281084975,4280756526,4280756269,4281084975,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3794611503,892153135,2960687,0,2960687,0,0,0,2960687,0,19737903,1445801263,4113378607,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4113378607,1445801263,19737903,0,2960687,0,0,0,0,0,2960687,0,86846767,1764568367,4214041903,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4214041903,1764568367,86846767,0,2960687,0,0,0,0,0,0,2960687,0,0,137178415,1764568367,4113378607,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4113378607,1764568367,137178415,0,0,2960687,0,0,0,0,0,0,0,2960687,0,0,86846767,1445801263,3794611503,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3794611503,1445801263,86846767,0,0,2960687,0,0,0,0,0,0,0,0,0,2960687,0,0,19737903,892153135,3089968431,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3089968431,892153135,19737903,0,0,2960687,0,0,0,0,0,0,0,0,0,0,0,2960687,2960687,0,2960687,321727791,1848454447,3928829231,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3928829231,1848454447,321727791,2960687,0,2960687,2960687,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2960687,0,0,36515119,556608815,2267884847,4046269743,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4046269743,2267884847,556608815,36515119,0,0,2960687,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2960687,0,0,2960687,36515119,372059439,1714236719,3375181103,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,4281150767,3375181103,1714236719,372059439,36515119,2960687,0,0,2960687,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2960687,0,0,2960687,19737903,0,422391087,1445801263,2402102575,3140300079,3710725423,4063046959,4247596335,4247596335,4063046959,3710725423,3140300079,2402102575,1445801263,422391087,0,19737903,2960687,0,0,2960687,0,0,0,0,0,0,0,0,0,0,0};

            //printf("%d\n", sizeof(buffer));
            //u64 buffer[] = {2,2,4294901760,4278255360,4292649817,4294967295};
            Atom net_wm_icon = XInternAtom(xlib_display, "_NET_WM_ICON", False);
            Atom cardinal = XInternAtom(xlib_display, "CARDINAL", False);
            i32 length = sizeof(buffer)/8;
            XChangeProperty(xlib_display, xlib_window, net_wm_icon, cardinal, 32, PropModeReplace, (u8*) buffer, length);


        }
    }

    // Create OpenGL context
    {
        GLXContext gl_context = glXCreateContext(xlib_display, xlib_visual_info,
                                                 0, GL_TRUE);
        glXMakeCurrent(xlib_display, xlib_window, gl_context);

        if (!gl_lite_init()) { exit(1); }

        glGetIntegerv(GL_MAJOR_VERSION, &mem->sys.gl_version[0]);
        glGetIntegerv(GL_MINOR_VERSION, &mem->sys.gl_version[1]);
        mem->sys.gl_vendor = (char*)glGetString(GL_VENDOR);
        mem->sys.gl_renderer = (char*)glGetString(GL_RENDERER);

        // Display vsync if possible
        typedef void SwapIntervalEXTproc(Display*, GLXDrawable, i32);
        SwapIntervalEXTproc* glXSwapIntervalEXT = 0;
        void* libGL = dlopen("libGL.so", RTLD_LAZY);
        if (libGL) {
            glXSwapIntervalEXT =
                (SwapIntervalEXTproc*) dlsym(libGL, "glXSwapIntervalEXT");
            if (glXSwapIntervalEXT) {
                glXSwapIntervalEXT(xlib_display, xlib_window, 0);
            }
        }
    }

    EasyTab_Load(xlib_display, xlib_window);
    easykey_init();

    core::init(mem);
    ImGui::GetIO().RenderDrawListsFn = core::render_imgui;

    timer::stop(Timer_Startup);

#ifdef PAPAYA_DEFAULT_IMAGE
    core::open_doc(PAPAYA_DEFAULT_IMAGE, mem);
#endif

    mem->is_running = true;

    while (mem->is_running) {
        timer::start(Timer_Frame);

        // Event handling
        while (XPending(xlib_display)) {
            XEvent event;
            XNextEvent(xlib_display, &event);

            if (EasyTab_HandleEvent(&event) == EASYTAB_OK) { continue; }
            if (easykey_handle_event(&event, xlib_display) == EASYKEY_OK) { continue; }

            switch (event.type) {
                case Expose: {
                    XWindowAttributes WindowAttributes;
                    XGetWindowAttributes(xlib_display, xlib_window, &WindowAttributes);
                    core::resize(mem, WindowAttributes.width,WindowAttributes.height);
                } break;

                case ClientMessage: {
                    if ((Atom)event.xclient.data.l[0] == xlib_delete_window_atom) {
                        mem->is_running = false;
                    }
                } break;

                case MotionNotify: {
                    ImGui::GetIO().MousePos.x = event.xmotion.x;
                    ImGui::GetIO().MousePos.y = event.xmotion.y;
                } break;

                case ButtonPress:
                case ButtonRelease: {
                    i32 Button = event.xbutton.button;
                    if 		(Button == 1) { Button = 0; } // This section maps Xlib's button indices (Left = 1, Middle = 2, Right = 3)
                    else if (Button == 2) { Button = 2; } // to Papaya's button indices              (Left = 0, Right = 1, Middle = 2)
                    else if (Button == 3) { Button = 1; } //

                    if (Button < 3)	{
                        ImGui::GetIO().MouseDown[Button] =
                            (event.type == ButtonPress);
                    } else {
                        if (event.type == ButtonPress) {
                            ImGui::GetIO().MouseWheel +=
                                (Button == 4) ?  +1.0f : -1.0f;
                        }
                    }
                } break;

            }
        }

        // Tablet input // TODO: Put this in papaya.cpp
        {
            mem->tablet.pressure = EasyTab->Pressure;
            mem->tablet.pos.x = EasyTab->PosX;
            mem->tablet.pos.y = EasyTab->PosY;
        }

        // Start new ImGui Frame
        {
            timespec Time;
            clock_gettime(CLOCK_MONOTONIC, &Time);
            f32 CurTime = Time.tv_sec + (f32)Time.tv_nsec / 1000000000.0f;
            ImGui::GetIO().DeltaTime = (f32)(CurTime - mem->profile.last_frame_time);
            mem->profile.last_frame_time = CurTime;

            ImGui::NewFrame();
        }

        // Update and render
        {
            core::update(mem);
            glXSwapBuffers(xlib_display, xlib_window);
        }

#ifdef USE_GTK
        // Run a GTK+ loop, and *don't* block if there are no events pending
        gtk_main_iteration_do(FALSE);
#endif

        // End Of Frame
        timer::stop(Timer_Frame);
        f64 FrameRate =
            (mem->current_tool == PapayaTool_Brush && mem->mouse.is_down[0]) ?
            500.0 : 60.0;
        f64 FrameTime = 1000.0 / FrameRate;
        f64 SleepTime = FrameTime - timers[Timer_Frame].elapsed_ms;
        timers[Timer_Sleep].elapsed_ms = SleepTime;
        if (SleepTime > 0) { usleep((u32)SleepTime * 1000); }
    }

    //core::destroy(&mem);
    EasyTab_Unload();

    return 0;
}
