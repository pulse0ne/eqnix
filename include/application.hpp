#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "pipeline.hpp"
#include "pa_manager.hpp"
#include "logger.hpp"
#include <gtkmm/application.h>

class Application : public Gtk::Application {
public:
    Application();
    ~Application();

    static Glib::RefPtr<Application> create();

    std::unique_ptr<PAManager> pam;
    std::unique_ptr<Pipeline> pipeline;

    logging::EqnixLogger logger = logging::EqnixLogger("Application");
private:
    bool running_as_service = false;

    void on_activate() override;
    void on_startup() override;
};

#endif // APPLICATION_HPP
