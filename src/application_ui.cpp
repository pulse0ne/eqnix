#include "application_ui.hpp"

ApplicationUI::ApplicationUI(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder, Application* application)
    : Gtk::ApplicationWindow(cobject), app(application) {
    colors = {
        {"background",        Gdk::RGBA("rgb( 33,  39,  44)")},
        {"accent",            Gdk::RGBA("rgb(255, 195,  14)")},
        {"fr-line",           Gdk::RGBA("rgb(  9, 184, 240)")},
        {"grid-lines",        Gdk::RGBA("rgb( 64,  64,  64)")},
        {"grid-lines-bright", Gdk::RGBA("rgb( 92,  92,  92)")}
    };
    builder->get_widget_derived("drawing_area", fr_plot, colors, application->pipeline->get_equalizer());
    logger.debug("initialized FR widget");

    set_default_size(400, 200);
}

ApplicationUI::~ApplicationUI() {}

ApplicationUI* ApplicationUI::create(Application* app) {
    auto builder = Gtk::Builder::create_from_resource("/com/github/pulse0ne/eqnix/application.glade");
    ApplicationUI* window = nullptr;

    builder->get_widget_derived("application_ui", window, app);
    logging::defaultLogger.debug("initialized main application window");

    return window;
}