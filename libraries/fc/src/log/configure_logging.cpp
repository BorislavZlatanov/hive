#include <fc/log/logger_config.hpp>
#include <fc/log/log_message.hpp>
#include <fc/log/console_appender.hpp>

#include <boost/thread/mutex.hpp>
#include <fc/thread/unique_lock.hpp>
#include <fc/thread/thread.hpp>
#include <fc/thread/task.hpp>

#include <fc/log/appender.hpp>
#include <fc/log/console_appender.hpp>
#include <fc/log/file_appender.hpp>

namespace fc {

bool configure_logging()
{
  static bool reg_console_appender = appender::register_appender<console_appender>( "console" );
  static bool reg_file_appender = appender::register_appender<file_appender>( "file" );

  return reg_console_appender || reg_file_appender;
}

const char* log_context::get_current_task_desc()const
{
  return fc::thread::current().current_task_desc();
}

void console_appender::log_impl(const std::string& line, color::type color)
{
  static boost::mutex m;
  fc::unique_lock<boost::mutex> lock(m);

  print( line, color );
  print_new_line();
}

} // namespace fc
