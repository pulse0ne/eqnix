#include "application.hpp"
#include "application_ui.hpp"
#include "config.h"
#include <glibmm.h>

Application::Application() : Gtk::Application("com.github.pulse0ne.eqnix") {
    Glib::set_application_name("eqnix");
    Glib::setenv("PULSE_PROP_application.id", "com.github.pulse0ne.eqnix");
    Glib::setenv("PULSE_PROP_application.icon_name", "com.github.pulse0ne.eqnix");
    logger.debug("in Application constructor");

    pam = std::make_unique<PAManager>();
    pipeline = std::make_unique<Pipeline>(pam.get()); // TODO this seems suspect
}

Application::~Application() {}

Glib::RefPtr<Application> Application::create() {
    auto app = Glib::RefPtr<Application>(new Application());
    app->logger.debug("in Application::create()");
    return app;
}

void Application::on_activate() {
    logger.debug("in Application::on_activate()");
    if (get_active_window() == nullptr) {
        auto window = ApplicationUI::create(this);

        add_window(*window);

        window->signal_hide().connect([&, window]() {
            int width, height;

            window->get_size(width, height);

            delete window;
        });

        window->show_all();

        pam->find_sink_inputs();
        // pam->find_source_outputs();
        pam->find_sinks();
        // pam->find_sources();
    }
}

void Application::on_startup() {
    Gtk::Application::on_startup();

    // pm = std::make_unique<PulseManager>();
}
