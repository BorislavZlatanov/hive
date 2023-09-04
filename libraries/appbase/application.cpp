#include <appbase/application.hpp>

#include <hive/utilities/logging_config.hpp>
#include <hive/utilities/notifications.hpp>
#include <hive/utilities/options_description_ex.hpp>

#include <fc/thread/thread.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/scope_exit.hpp>

#include <iostream>
#include <fstream>
#include <thread>

namespace appbase {

class quoted
{
public:
  explicit quoted(const char* s)
    : data(s)
  {}

  friend std::ostream& operator<<(std::ostream& stream, const quoted& q)
  {
    return stream << "\"" << q.data << "\"";
  }
private:
  const char* data;
};

namespace bpo = boost::program_options;
using bpo::options_description;
using bpo::variables_map;
using hive::utilities::options_description_ex;
using std::cout;

io_handler::io_handler(application& _app, bool _allow_close_when_signal_is_received, final_action_type&& _final_action )
      : allow_close_when_signal_is_received( _allow_close_when_signal_is_received ), final_action( _final_action ), app(_app)
{
}

io_handler::~io_handler()
{
  /*
    The best moment for clearing signals is time of destroying an object then for sure we don't need signals.
    Otherwise can be a situation that signals were removed, but an application gets another SIGINT and that signal won't be able to be process correctly.
  */
  clear_signals();
}

boost::asio::io_service& io_handler::get_io_service()
{
  return io_serv;
}

void io_handler::close()
{
  /** Since signals can be processed asynchronously, it would be possible to call this twice
  *    concurrently while op service is stopping.
  */
  while( lock.test_and_set( std::memory_order_acquire ) )
    ;

  if( !closed )
  {
    final_action();

    io_serv.stop();

    closed = true;
  }

  lock.clear( std::memory_order_release );
}

void io_handler::clear_signals()
{
  if( !signals )
    return;

  boost::system::error_code ec;
  signals->cancel( ec );
  signals.reset();

  if( ec.value() != 0 )
    cout<<"Error during cancelling signal: "<< ec.message() << std::endl;
}

void application::generate_interrupt_request()
{
  notify_status("interrupted");
  _is_interrupt_request = true;
  ilog("interrupt requested!");
}

void io_handler::handle_signal( uint32_t _last_signal_code )
{
  last_signal_code = _last_signal_code;

  if(_last_signal_code == SIGINT || _last_signal_code == SIGTERM)
  {
    idump((_last_signal_code));
    app.generate_interrupt_request();
  }

  if( allow_close_when_signal_is_received )
    close();
}

void io_handler::attach_signals()
{
  /** To avoid killing process by broken pipe and continue regular app shutdown.
    *  Useful for usecase: `hived | tee hived.log` and pressing Ctrl+C
    **/
  signal(SIGPIPE, SIG_IGN);

  signals = p_signal_set( new boost::asio::signal_set( io_serv, SIGINT, SIGTERM ) );
  signals->async_wait([ this ](const boost::system::error_code& err, int signal_number ) {
  /// Handle signal only if it was really present (it is possible to get error_code for 'Operation cancelled' together with signal_number == 0)
  if(signal_number != 0)
    handle_signal( signal_number );
  });
}

void io_handler::run()
{
  io_serv.run();
}

class application_impl {
  public:
    application_impl() : 
      _app_options("Application Options"),
      _logging_thread("logging_thread")
    {}
    const variables_map*    _options = nullptr;
    options_description_ex  _app_options;
    options_description_ex  _cfg_options;
    variables_map           _args;

    bfs::path               _data_dir;

    fc::thread              _logging_thread;
};

application::application()
: pre_shutdown_plugins(
  []( abstract_plugin* a, abstract_plugin* b )
  {
    assert( a && b );
    return a->get_pre_shutdown_order() > b->get_pre_shutdown_order();
  }
),
  my(new application_impl()), main_io_handler(*this, true/*allow_close_when_signal_is_received*/, [ this ](){ finish(); } )
{
}

application::~application() { }

fc::optional< fc::logging_config > application::load_logging_config()
{
  fc::optional< fc::logging_config > logging_config;
  const variables_map& args = get_args();
  my->_logging_thread.async( [args, &logging_config]{
    try
    {
      logging_config = 
        hive::utilities::load_logging_config( args, appbase::app().data_dir() );
      if( logging_config )
        fc::configure_logging( *logging_config );
    }
    catch( const fc::exception& e )
    {
      wlog( "Error parsing logging config. ${e}", ("e", e.to_string()) );
    }
  }).wait();
  ilog("Logging thread started");
  return logging_config;
}

void application::startup() {

  std::cout << "Setting up a startup_io_handler..." << std::endl;

  io_handler startup_io_handler(*this, false/*allow_close_when_signal_is_received*/, []() {});
  startup_io_handler.attach_signals();

  std::thread startup_thread = std::thread( [&startup_io_handler]()
  {
    startup_io_handler.run();
  });

  BOOST_SCOPE_EXIT(&startup_io_handler, &startup_thread)
  {
    startup_io_handler.close();
    startup_thread.join();
  } BOOST_SCOPE_EXIT_END

  for (const auto& plugin : initialized_plugins)
  {
    plugin->startup();

    if( is_interrupt_request() )
      break;
  }

  if( !is_interrupt_request() )
  {
    for( const auto& plugin : initialized_plugins )
    {
      plugin->finalize_startup();
    }
  }
}

application& application::instance( bool reset ) {
  static application* _app = new application();
  if( reset || _app->is_finished )
  {
    delete _app;
    _app = new application();
  }
  return *_app;
}
application& app() { return application::instance(); }
application& reset() { return application::instance( true ); }


void application::set_program_options()
{
  std::stringstream data_dir_ss;
  data_dir_ss << "Directory containing configuration file config.ini. Default location: $HOME/." << app_name << " or CWD/. " << app_name;

  std::stringstream plugins_ss;
  for( auto& p : default_plugins )
  {
    plugins_ss << p << ' ';
  }

  options_description_ex app_cfg_opts( "Application Config Options" );
  options_description_ex app_cli_opts( "Application Command Line Options" );
  app_cfg_opts.add_options()
      ("plugin", bpo::value< vector<string> >()->composing()->default_value(default_plugins, plugins_ss.str())->value_name("plugin-name"), "Plugin(s) to enable, may be specified multiple times");

  app_cli_opts.add_options()
      ("help,h", "Print this help message and exit.")
      ("version,v", "Print version information.")
      ("dump-config", "Dump configuration and exit")
      ("list-plugins", "Print names of all available plugins and exit")
#if BOOST_VERSION >= 106800
      ("generate-completions", "Generate bash auto-complete script (try: eval \"$(hived --generate-completions)\")")
#endif
      ("data-dir,d", bpo::value<bfs::path>()->value_name("dir"), data_dir_ss.str().c_str() )
      ("config,c", bpo::value<bfs::path>()->default_value("config.ini")->value_name("filename"), "Configuration file name relative to data-dir");

  my->_cfg_options.add(app_cfg_opts);
  my->_app_options.add(app_cfg_opts);
  my->_app_options.add(app_cli_opts);

  set_plugin_options( &my->_app_options, &my->_cfg_options );
}

void application::set_plugin_options(
  bpo::options_description*  app_options,
  bpo::options_description*  cfg_options ) const
{
  for(auto& plug : plugins) {
    options_description_ex plugin_cli_opts("Command Line Options for " + plug.second->get_name());
    options_description_ex plugin_cfg_opts("Config Options for " + plug.second->get_name());
    plug.second->set_program_options(plugin_cli_opts, plugin_cfg_opts);
    if(plugin_cli_opts.options().size())
      app_options->add(plugin_cli_opts);
    if(plugin_cfg_opts.options().size())
    {
      cfg_options->add(plugin_cfg_opts);

      for(const boost::shared_ptr<bpo::option_description>& od : plugin_cfg_opts.options())
      {
        // If the config option is not already present as a cli option, add it.
        if( plugin_cli_opts.find_nothrow( od->long_name(), false ) == nullptr )
        {
          app_options->add( od );
        }
      }
    }
  }
}

initialization_result application::initialize_impl( int argc, char** argv, 
  vector<abstract_plugin*> autostart_plugins, const bpo::variables_map& arg_overrides )
{
  try
  {
    // Due to limitations of boost::program_options::store function (used twice below),
    // the overrides need to land first in variables_map container (_args), before actual
    // args to be overridden are processed.
    my->_args = arg_overrides;
    set_program_options();
    bpo::store( bpo::parse_command_line( argc, argv, my->_app_options ), my->_args );

    if( my->_args.count( "help" ) ) {
      cout << my->_app_options << "\n";
      return { initialization_result::ok, false };
    }

    if( my->_args.count( "version" ) )
    {
      cout << version_info << "\n";
      return { initialization_result::ok, false };
    }

    bfs::path data_dir;
    if( my->_args.count("data-dir") )
    {
      data_dir = my->_args["data-dir"].as<bfs::path>();
      if( data_dir.is_relative() )
        data_dir = bfs::current_path() / data_dir;
    }
    else
    {
#ifdef WIN32
      char* parent = getenv( "APPDATA" );
#else
      char* parent = getenv( "HOME" );
#endif

      if( parent != nullptr )
      {
        data_dir = std::string( parent );
      }
      else
      {
        data_dir = bfs::current_path();
      }

      std::stringstream app_dir;
      app_dir << '.' << app_name;

      data_dir = data_dir / app_dir.str();
    }
    my->_data_dir = data_dir;

    bfs::path config_file_name = data_dir / "config.ini";
    if( my->_args.count( "config" ) ) {
      config_file_name = my->_args["config"].as<bfs::path>();
      if( config_file_name.is_relative() )
        config_file_name = data_dir / config_file_name;
    }

    if(!bfs::exists(config_file_name)) {
      write_default_config(config_file_name);
    }

#if BOOST_VERSION >= 106800
    if(my->_args.count("generate-completions") > 0)
    {
      generate_completions();
      return { initialization_result::ok, false };
    }
#endif

    bpo::store(bpo::parse_config_file< char >( config_file_name.make_preferred().string().c_str(),
                              my->_cfg_options, true ), my->_args );

    if(my->_args.count("dump-config") > 0)
    {
      std::cout << "{\n";
      std::cout << "\t" << quoted("data-dir") << ": " << quoted(my->_data_dir.string().c_str()) << ",\n";
      std::cout << "\t" << quoted("config") << ": " << quoted(config_file_name.string().c_str()) << "\n";
      std::cout << "}\n";
      return { initialization_result::ok, false };
    }

    if(my->_args.count("list-plugins") > 0)
    {
      for(const auto& plugin: plugins)
      {
        std::cout << plugin.first << "\n";
      }

      return { initialization_result::ok, false };
    }

    if(my->_args.count("plugin") > 0)
    {
      auto plugins = my->_args.at("plugin").as<std::vector<std::string>>();
      for(auto& arg : plugins)
      {
        vector<string> names;
        boost::split(names, arg, boost::is_any_of(" \t,"));
        for(const std::string& name : names)
          get_plugin(name).initialize(my->_args);
      }
    }
    for (const auto& plugin : autostart_plugins)
      if (plugin != nullptr && plugin->get_state() == abstract_plugin::registered)
        plugin->initialize(my->_args);

    bpo::notify(my->_args);

    return { initialization_result::ok, true };
  }
  catch (const boost::program_options::validation_error& e)
  {
    std::cerr << "Error parsing command line: " << e.what() << "\n";
    return { initialization_result::validation_error, false };
  }
  catch (const boost::program_options::unknown_option& e)
  {
    std::cerr << "Error parsing command line: " << e.what() << "\n";
    return { initialization_result::unrecognised_option, false };
  }
  catch (const boost::program_options::error_with_option_name& e)
  {
    std::cerr << "Error parsing command line: " << e.what() << "\n";
    return { initialization_result::error_with_option, false };
  }
  catch (const boost::program_options::error& e)
  {
    std::cerr << "Error parsing command line: " << e.what() << "\n";
    return { initialization_result::unknown_command_line_error, false };
  }
}

void application::pre_shutdown( std::string& actual_plugin_name )
{
  std::cout << "Before shutting down...\n";

  for( auto& plugin : pre_shutdown_plugins )
  {
    actual_plugin_name = plugin->get_name();
    plugin->pre_shutdown();
  }

  pre_shutdown_plugins.clear();
}

void application::shutdown( std::string& actual_plugin_name ) {

  std::cout << "Shutting down...\n";

  for(auto ritr = running_plugins.rbegin();
      ritr != running_plugins.rend(); ++ritr) {
    actual_plugin_name = (*ritr)->get_name();
    (*ritr)->shutdown();
  }
  for(auto ritr = running_plugins.rbegin();
      ritr != running_plugins.rend(); ++ritr) {
    actual_plugin_name = (*ritr)->get_name();
    plugins.erase( actual_plugin_name );
  }
  running_plugins.clear();
  initialized_plugins.clear();
  plugins.clear();
}

void application::finish()
{
  std::string _actual_plugin_name;

  auto plugin_exception_info = [&_actual_plugin_name]()
  {
    std::cerr << "Plugin: "<< _actual_plugin_name <<" raised an exception..."<< "\n";
  };

  try
  {
    std::cout << "Executing `pre shutdown` for all plugins..." << "\n";
    pre_shutdown( _actual_plugin_name );

    std::cout << "Executing `shutdown` for all plugins..." << "\n";
    shutdown( _actual_plugin_name );

    is_finished = true;

    fc::promise<void>::ptr quitDone( new fc::promise<void>("Logging thread quit") );
    my->_logging_thread.quit( quitDone.get() );
    ilog("Waiting for logging_thread quit");
    quitDone->wait();
    ilog("logging_thread quit done");
  }
  catch ( const boost::exception& e )
  {
    plugin_exception_info();
    std::cerr << boost::diagnostic_information(e) << "\n";
  }
  catch( std::exception& e )
  {
    plugin_exception_info();
    std::cerr << ("exception: ") << e.what() << std::endl;
  }
  catch(...)
  {
    plugin_exception_info();
    try
    {
      std::rethrow_exception( std::current_exception() );
    }
    catch( const std::exception& e )
    {
      std::cerr << ("exception: ") << e.what() << std::endl;
    }
  }
}

void application::exec()
{
  std::cout << ("Entering application main loop...") << std::endl;

  if( !is_interrupt_request() )
  {
    main_io_handler.attach_signals();

    notify_status("signals attached");

    main_io_handler.run();
  }
  else
  {
    std::cout << ("performing shutdown on interrupt request...") << std::endl;
    finish();
  }

  std::cout << ("Leaving application main loop...") << std::endl;
}

void application::write_default_config(const bfs::path& cfg_file)
{
  if(!bfs::exists(cfg_file.parent_path()))
    bfs::create_directories(cfg_file.parent_path());

  std::ofstream out_cfg( bfs::path(cfg_file).make_preferred().string());
  for(const boost::shared_ptr<bpo::option_description>& od : my->_cfg_options.options())
  {
    if(!od->description().empty())
    {
      std::string next_line;
      std::stringstream ss( od->description() );
      while( getline(ss, next_line, '\n') )
        out_cfg << "# " << next_line << "\n";
    }
    boost::any store;
    if(!od->semantic()->apply_default(store))
      out_cfg << "# " << od->long_name() << " = \n";
    else
    {
      auto example = od->format_parameter();
      if( example.empty() )
      {
        // This is a boolean switch
        out_cfg << od->long_name() << " = " << "false\n";
      }
      else if( example.length() <= 7 )
      {
        // The string is formatted "arg"
        out_cfg << "# " << od->long_name() << " = \n";
      }
      else
      {
        // The string is formatted "arg (=<interesting part>)"
        size_t space_pos = example.find(' ');
        if (space_pos != std::string::npos && 
            example.length() >= space_pos + 4)
        {
          example.erase(0, space_pos + 3);
          example.erase(example.length()-1);
          out_cfg << od->long_name() << " = " << example << "\n";
        }
      }
    }
    out_cfg << "\n";
  }
  out_cfg.close();
}

void application::generate_completions()
{
  // option_description is missing long_names() before 1.68.0, and it's easier to just omit this
  // feature for old versions of boost, instead of trying to support it
#if BOOST_VERSION >= 106800
  std::vector<string> all_plugin_names;
  for (const auto& plugin: plugins)
    all_plugin_names.push_back(plugin.first);

  // generate a string containing all options, separated by a space
  std::vector<std::string> all_options;
  std::vector<string> args_that_take_plugin_names;
  std::vector<string> args_that_take_directories_names;
  std::vector<string> args_that_have_no_completion;
  for (const boost::shared_ptr<bpo::option_description>& od : my->_app_options.options())
  {
    std::vector<std::string> this_parameter_variations;
    // option_description doesn't have a direct acessor for the short option, so
    // call format_name() which will return a combined short + long "-h [ --help ]"
    // string, then parse the short option out
    std::string formatted = od->format_name();
    if (formatted.length() > 2 && 
        formatted[0] == '-' &&
        formatted[1] != '-')
      this_parameter_variations.push_back(formatted.substr(0, 2));

    const std::string* long_name_strings;
    size_t long_name_count;
    std::tie(long_name_strings, long_name_count) = od->long_names();
    for (unsigned i = 0; i < long_name_count; ++i)
      this_parameter_variations.push_back(std::string("--" + long_name_strings[i]));

    std::copy(this_parameter_variations.begin(), this_parameter_variations.end(), std::back_inserter(all_options));

    // we don't have a direct way to get the value_name, so parse it from the 
    // format_parameter() output
    std::string formatted_parameter = od->format_parameter();
    size_t space_pos = formatted_parameter.find(' ');
    if (space_pos != std::string::npos || !formatted_parameter.empty())
    {
      std::string value_name;
      if (space_pos == std::string::npos)
        value_name = formatted_parameter;
      else
        value_name = formatted_parameter.substr(0, space_pos);
      if (value_name == "plugin-name")
        std::copy(this_parameter_variations.begin(), this_parameter_variations.end(), std::back_inserter(args_that_take_plugin_names));
      else if (value_name == "dir")
        std::copy(this_parameter_variations.begin(), this_parameter_variations.end(), std::back_inserter(args_that_take_directories_names));
      else
        std::copy(this_parameter_variations.begin(), this_parameter_variations.end(), std::back_inserter(args_that_have_no_completion));
    }
  }

  std::cout << "_hived()\n"
            << "{\n" 
            << "  local hived=$1 cur=$2 prev=$3 words=(\"${COMP_WORDS[@]}\")\n"
            << "  case \"${prev}\" in\n";
  if (!args_that_take_plugin_names.empty())
    std::cout << "    " << boost::algorithm::join(args_that_take_plugin_names, "|") << ")\n"
              << "      COMPREPLY=( $(compgen -W \"" << boost::algorithm::join(all_plugin_names, " ") << "\" -- \"$cur\") )\n"
              << "      return\n"
              << "      ;;\n";
  if (!args_that_take_directories_names.empty())
    std::cout << "    " << boost::algorithm::join(args_that_take_directories_names, "|") << ")\n"
              << "      COMPREPLY=( $(compgen -A directory -- \"$cur\") )\n"
              << "      return\n"
              << "      ;;\n";
  std::cout << "    " << boost::algorithm::join(args_that_have_no_completion, "|") << ")\n"
            << "      COMPREPLY=()\n"
            << "      return\n"
            << "      ;;\n"
            << "  esac\n"
            << "  COMPREPLY=( $(compgen -W \"" << boost::algorithm::join(all_options, " ") << "\" -- \"$cur\") )\n"
            << "}\n"
            << "complete -F _hived -o filenames hived\n";
#endif
}

abstract_plugin* application::find_plugin( const string& name )const
{
  auto itr = plugins.find( name );

  if( itr == plugins.end() )
  {
    return nullptr;
  }

  return itr->second.get();
}

abstract_plugin& application::get_plugin(const string& name)const
{
  auto ptr = find_plugin(name);
  if(!ptr)
  {
    notify_error("Unable to find plugin: " + name);
    BOOST_THROW_EXCEPTION(std::runtime_error("unable to find plugin: " + name));
  }
  return *ptr;
}

bfs::path application::data_dir()const
{
  return my->_data_dir;
}

void application::add_program_options( const options_description& cli, const options_description& cfg )
{
  my->_app_options.add( cli );
  my->_app_options.add( cfg );
  my->_cfg_options.add( cfg );
}

void application::add_logging_program_options()
{
  hive::utilities::options_description_ex options;
  hive::utilities::set_logging_program_options( options );
  hive::utilities::notifications::add_program_options(options);
  
  add_program_options( hive::utilities::options_description_ex(), options );
}

const variables_map& application::get_args() const
{
  return my->_args;
}

std::set< std::string > application::get_plugins_names() const
{
  std::set< std::string > res;

  for( auto& plugin : initialized_plugins )
    res.insert( plugin->get_name() );

  return res;
}

void application::notify_status(const fc::string& current_status) const noexcept
{
  notify("hived_status", "current_status", current_status);
}

void application::notify_error(const fc::string& error_message) const noexcept
{
  notify("error", "message", error_message);
}

void application::setup_notifications(const boost::program_options::variables_map &args) const
{
  notification_handler.setup( hive::utilities::notifications::setup_notifications( args ) );
}


} /// namespace appbase
