#include "rosco.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <syslog.h>

#ifdef WIN32
#include <windows.h>
#endif

#if defined(__arm__)
#include <wiringPi.h>
#endif

static unsigned int o_gpiopin = 0;

/* Update the LED */
void led(int on)
{
#if defined(__arm__)
  static int current = 1; /* Ensure the LED turns off on first call */
  if (current == on)
    return;

  if (on)
  {
    digitalWrite(o_gpiopin, HIGH);
  }
  else
  {
    digitalWrite(o_gpiopin, LOW);
  }

  current = on;
#endif
}

/* LED setup */
void led_setup()
{
#if defined(__arm__)
  wiringPiSetup();
  pinMode(o_gpiopin, OUTPUT);
  /* Ensure the LED is off */
  led(LOW);
  printf("LED connection signalling established.\n");
#endif
}

enum command_idx
{
  MC_Read = 0,
  MC_Read_Raw = 1,
  MC_Read_IAC = 2,
  MC_PTC = 3,
  MC_FuelPump = 4,
  MC_IAC_Close = 5,
  MC_IAC_Open = 6,
  MC_AC = 7,
  MC_Coil = 8,
  MC_Injectors = 9,
  MC_Interactive = 10,
  MC_Num_Commands = 11
};

static const char *commands[] = {
    "read",
    "read-raw",
    "read-iac",
    "ptc",
    "fuelpump",
    "iac-close",
    "iac-open",
    "ac",
    "coil",
    "injectors",
    "interactive"};

char *simple_current_time(void)
{
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  struct timeval tv;
  static char buffer[50];

  gettimeofday(&tv, NULL);

  sprintf(buffer, "%02d:%02d:%02d.000", tm.tm_hour, tm.tm_min, tm.tm_sec);
  return buffer;
}

bool prefix(const char pre, const char *str)
{
  return (str[0] == pre);
}

int find_command(char *command)
{
  int cmd_idx = 0;

  while ((cmd_idx < MC_Num_Commands) && (strcasecmp(command, commands[cmd_idx]) != 0))
  {
    cmd_idx += 1;
  }

  return cmd_idx;
}

int read_config(readmems_config *config, char *path)
{
  FILE *file;
  char line[200];
  char *key;
  char *value;
  char *search = "=";
  char comment = '#';
  int cmd_idx = -1;
  int len;

  file = fopen(path, "r");
  config->port = strdup("ttyecu");
  config->command = strdup("read");
  config->output = strdup("stdout");
  config->loop = strdup("inf");
  config->connection = strdup("nowait");

  if (file)
  {
    printf("using config from %s\n", path);
    syslog(LOG_NOTICE, "using config from %s", path);

    while (fgets(line, sizeof(line), file))
    {
      if (strlen(line) > 1)
      {
        if (prefix(comment, line))
        {
          // skip comments
        }
        else
        {
          key = strtok(line, search);
          value = strtok(NULL, search);

          // remove newline
          len = strlen(value);
          if (value[len - 1] == '\n')
            value[len - 1] = 0;

          if (strcasecmp(key, "port") == 0)
          {
            config->port = strdup(value);
          }

          if (strcasecmp(key, "command") == 0)
          {
            config->command = strdup(value);
          }

          if (strcasecmp(key, "output") == 0)
          {
            config->output = strdup(value);
          }

          if (strcasecmp(key, "loop") == 0)
          {
            config->loop = strdup(value);
          }

          if (strcasecmp(key, "connection") == 0)
          {
            config->connection = strdup(value);
          }
        }
      }
    }

    cmd_idx = find_command(config->command);
  }
  else
  {
    printf("no readmems.cfg found, using defaults\n");
    syslog(LOG_NOTICE, "no readmes.cfg, using defaults");
  }

  fclose(file);

  return cmd_idx;
}

char *current_date(void)
{
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  static char buffer[50];

  sprintf(buffer, "%4d-%02d-%02d-%02d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  return buffer;
}

void sleep_ms(int milliseconds) // cross-platform sleep function
{
#ifdef WIN32
  Sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
  struct timespec ts;
  ts.tv_sec = milliseconds / 1000;
  ts.tv_nsec = (milliseconds % 1000) * 1000000;
  nanosleep(&ts, NULL);
#else
  usleep(milliseconds * 1000);
#endif
}

char *open_log_file(FILE **fp)
{
  static char filename[200];

  sprintf(filename, "readmems-%s.csv", current_date());

  // open the file for writing
  *fp = fopen(filename, "w");

  return filename;
}

char *split_log_file(FILE **fp, int size)
{
  char *filename;

  if (*fp)
  {
    // min size 10000Kb
    if (size < 10000)
    {
      size = 10000;
    }

    // close existing file and open a new file
    if (get_file_size(*fp) > size)
    {
      fclose(*fp);
      filename = open_log_file(fp);
      write_memsscan_header(*fp);

      printf("split log file, new file created %s\n\n", filename);
      syslog(LOG_NOTICE, "split log file, new file created %s\n\n", filename);
    }
  }

  // return NULL if no new file
  return NULL;
}

int write_log(FILE **fp, char *line)
{
  if (*fp)
    return fprintf(*fp, "%s", line);
  else
    return -1;
}

int get_file_size(FILE *fp)
{
  if (fp)
  {
    fseek(fp, 0L, SEEK_END);

    return ftell(fp);
  }
  else
  {
    return -1;
  }
}

char *write_memsscan_header(FILE *fp)
{
  static char header[1024];

  // create header
  sprintf(header, "#time,"
                  "80x01-02_engine-rpm,80x03_coolant_temp,80x04_ambient_temp,80x05_intake_air_temp,80x06_fuel_temp,80x07_map_kpa,80x08_battery_voltage,80x09_throttle_pot,80x0A_idle_switch,80x0B_uk1,"
                  "80x0C_park_neutral_switch,80x0D-0E_fault_codes,80x0F_idle_set_point,80x10_idle_hot,80x11_uk2,80x12_iac_position,80x13-14_idle_error,80x15_ignition_advance_offset,80x16_ignition_advance,80x17-18_coil_time,"
                  "80x19_crankshaft_position_sensor,80x1A_uk4,80x1B_uk5,"
                  "7dx01_ignition_switch,7dx02_throttle_angle,7dx03_uk6,7dx04_air_fuel_ratio,7dx05_dtc2,7dx06_lambda_voltage,7dx07_lambda_sensor_frequency,7dx08_lambda_sensor_dutycycle,7dx09_lambda_sensor_status,7dx0A_closed_loop,"
                  "7dx0B_long_term_fuel_trim,7dx0C_short_term_fuel_trim,7dx0D_carbon_canister_dutycycle,7dx0E_dtc3,7dx0F_idle_base_pos,7dx10_uk7,7dx11_dtc4,7dx12_ignition_advance2,7dx13_idle_speed_offset,7dx14_idle_error2,"
                  "7dx14-15_uk10,7dx16_dtc5,7dx17_uk11,7dx18_uk12,7dx19_uk13,7dx1A_uk14,7dx1B_uk15,7dx1C_uk16,7dx1D_uk17,7dx1E_uk18,7dx1F_uk19,0x7d_raw,0x80_raw\n");

  printf("%s", header);

  // write to log file if enabled
  if (fp)
    write_log(&fp, header);

  return header;
}

void delete_file(char *filename)
{
  remove(filename);
}

void printbuf(uint8_t *buf, unsigned int count)
{
  unsigned int idx = 0;

  while (idx < count)
  {
    idx += 1;
    printf("%02X ", buf[idx - 1]);
    if (idx % 16 == 0)
    {
      printf("\n");
    }
  }
  printf("\n");
}

int16_t readserial(mems_info *info, uint8_t *buffer, uint16_t quantity)
{
  int16_t bytesread = -1;

#if defined(WIN32)
  DWORD w32BytesRead = 0;

  if ((ReadFile(info->sd, (UCHAR *)buffer, quantity, &w32BytesRead, NULL) == TRUE) && (w32BytesRead > 0))
  {
    bytesread = w32BytesRead;
  }
#else
  bytesread = read(info->sd, buffer, quantity);
#endif

  return bytesread;
}

int16_t writeserial(mems_info *info, uint8_t *buffer, uint16_t quantity)
{
  int16_t byteswritten = -1;

#if defined(WIN32)
  DWORD w32BytesWritten = 0;

  if ((WriteFile(info->sd, (UCHAR *)buffer, quantity, &w32BytesWritten, NULL) == TRUE) &&
      (w32BytesWritten == quantity))
  {
    byteswritten = w32BytesWritten;
  }
#else
  byteswritten = write(info->sd, buffer, quantity);
#endif

  return byteswritten;
}

bool interactive_mode(mems_info *info, uint8_t *response_buffer)
{
  size_t icmd_size = 8;
  char *icmd_buf_ptr;
  uint8_t icmd;
  ssize_t bytes_read = 0;
  ssize_t total_bytes_read = 0;
  bool quit = false;

  if ((icmd_buf_ptr = (char *)malloc(icmd_size)) != NULL)
  {
    printf("Enter a command (in hex) or 'quit'.\n> ");
    while (!quit && (fgets(icmd_buf_ptr, icmd_size, stdin) != NULL))
    {
      if ((strncmp(icmd_buf_ptr, "q", 1) == 0) ||
          (strncmp(icmd_buf_ptr, "quit", 4) == 0))
      {
        quit = true;
      }
      else if (icmd_buf_ptr[0] != '\n' && icmd_buf_ptr[1] != '\r')
      {
        icmd = strtoul(icmd_buf_ptr, NULL, 16);
        if ((icmd >= 0) && (icmd <= 0xff))
        {
          if (writeserial(info, &icmd, 1) == 1)
          {
            bytes_read = 0;
            total_bytes_read = 0;
            do
            {
              bytes_read = readserial(info, response_buffer + total_bytes_read, 1);
              total_bytes_read += bytes_read;
            } while (bytes_read > 0);

            if (total_bytes_read > 0)
            {
              printbuf(response_buffer, total_bytes_read);
            }
            else
            {
              printf("No response from ECU.\n");
              syslog(LOG_ERR, "No response from ECU.");
            }
          }
          else
          {
            printf("Error: failed to write command byte to serial port.\n");
            syslog(LOG_ERR, "Error: failed to write command byte to serial port.");
          }
        }
        else
        {
          printf("Error: command must be between 0x00 and 0xFF.\n");
        }
        printf("> ");
      }
      else
      {
        printf("> ");
      }
    }

    free(icmd_buf_ptr);
  }
  else
  {
    printf("Error allocating command buffer memory.\n");
  }

  return quit;
}

int main(int argc, char **argv)
{
  bool success = false;
  int cmd_idx = -1;
  mems_data data;
  mems_data_frame_80 frame80;
  mems_data_frame_7d frame7d;
  librosco_version ver;
  mems_info info;
  uint8_t *frameptr;
  uint8_t bufidx;
  uint8_t readval = 0;
  uint8_t iac_limit_count = 80; // number of times to re-send an IAC move command when
  char log_line[1024];
  char *port;
  bool connected = false;
  bool wait_for_connection = false;
  bool log_to_file = false;
  FILE *fp = NULL;
  char *config_file;

  // raspberry pi LED signalling on GPIO
  led_setup();

  // set up syslog
  setlogmask(LOG_UPTO(LOG_NOTICE));
  openlog("readmems", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

  // the ECU is already reporting that the valve has
  // reached its requested position
  int read_loop_count = 1;
  bool read_inf = false;

  // this is twice as large as the micro's on-chip ROM, so it's probably sufficient
  uint8_t response_buffer[16384];

  char win32devicename[16];

  ver = mems_get_lib_version();

  // read the config file for defaults
  readmems_config config;
  config_file = strdup("readmems.cfg");

  // if only 1 argument then expect the config file path
  if (argc == 2)
  {
    config_file = argv[1];
  }

  cmd_idx = read_config(&config, config_file);

  // if the config is invalid or any parameters have been specified on the command line
  // override the config
  if ((argc > 2) || (cmd_idx == -1))
  {
    // show info if the command line is invalid
    if (argc < 3)
    {
      printf("readmems using librosco v%d.%d.%d\n", ver.major, ver.minor, ver.patch);
      printf("Diagnostic utility using ROSCO protocol for MEMS 1.6 systems\n");
      printf("Usage: %s <serial device> <command> [read-loop-count]\n", basename(argv[0]));
      printf(" where <command> is one of the following:\n");
      for (cmd_idx = 0; cmd_idx < MC_Num_Commands; ++cmd_idx)
      {
        printf("\t%s\n", commands[cmd_idx]);
      }
      printf(" and [read-loop-count] is either a number or 'inf' to read forever.\n");

      return 0;
    }

    // find the index of the command to run
    while ((cmd_idx < MC_Num_Commands) && (strcasecmp(argv[2], commands[cmd_idx]) != 0))
    {
      cmd_idx += 1;
    }

    if (cmd_idx >= MC_Num_Commands)
    {
      printf("Invalid command: %s\n", argv[2]);
      return -1;
    }

    if (argc >= 4)
    {
      if (strcmp(argv[3], "inf") == 0)
      {
        read_inf = true;
      }
      else
      {
        read_loop_count = strtoul(argv[3], NULL, 0);
      }
    }

    // assign the serial port
    port = argv[1];
  }
  else
  {
    printf("Using config:\nport: %s\ncommand: %s (%d)\noutput: %s\nloop: %s\n", config.port, config.command, cmd_idx, config.output, config.loop);
    syslog(LOG_NOTICE, "Using config {port: %s, command: %s (%d), output: %s, loop: %s}", config.port, config.command, cmd_idx, config.output, config.loop);

    // set how many times to loop the command
    // 'inf' for infinite
    if (strcmp(config.loop, "inf") == 0)
    {
      read_inf = true;
    }
    else
    {
      read_loop_count = strtoul(config.loop, NULL, 0);
    }

    // wait until connection becomes available
    // this is useful when connecting to MEMS as it will retry until
    // the ECU is powered on
    if (strcmp(config.connection, "wait") == 0)
    {
      wait_for_connection = true;
    }

    // automatically save output to file rather than having to pipe stdio
    if (strcmp(config.output, "stdout") != 0)
    {
      log_to_file = true;
    }

    // assign the serial port from configuration file
    port = config.port;
  }

  if (cmd_idx != MC_Interactive)
  {
    printf("Running command: %s\n", commands[cmd_idx]);
    syslog(LOG_NOTICE, "Running command: %s", commands[cmd_idx]);
  }

  mems_init(&info);

#if defined(WIN32)
  // correct for microsoft's legacy nonsense by prefixing with "\\.\"
  strcpy(win32devicename, "\\\\.\\");
  strncat(win32devicename, config.port, 16);
  port = win32devicename;
#else
  port = config.port;
#endif

  do
  {
    led(1);

    printf("attempting to connect to %s\n", port);
    syslog(LOG_NOTICE, "attempting to connect to %s", port);
    connected = mems_connect(&info, port);

    if (!connected)
    {
      led(0);

      // not connected, so pause and retry
      if (wait_for_connection)
      {
        printf("waiting to retry connection to %s\n", port);
        syslog(LOG_NOTICE, "waiting to retry connection to %s", port);
        sleep(2);
      }
    }
    else
    {
      // connected, no need to wait anymore
      wait_for_connection = false;
    }
  } while ((wait_for_connection) && (!connected));

  if (connected)
  {
    led(0);
    led(1);
    led(0);
    led(1);
    led(0);
    led(1);
    led(0);

    if (log_to_file)
    {
      // open the log file if logging enabled
      // the full path get stored in config.output variable
      config.output = open_log_file(&fp);
      printf("logging to %s\n", config.output);
      syslog(LOG_NOTICE, "logging to %s\n", config.output);
    }

    if (mems_init_link(&info, response_buffer))
    {
      printf("ECU responded to D0 command with: %02X %02X %02X %02X\n\n",
             response_buffer[0], response_buffer[1], response_buffer[2], response_buffer[3]);

      syslog(LOG_NOTICE, "ECU responded to D0 command with: %02X %02X %02X %02X\n\n",
             response_buffer[0], response_buffer[1], response_buffer[2], response_buffer[3]);

      switch (cmd_idx)
      {
      case MC_Read:
        // create header
        write_memsscan_header(fp);

        while (read_inf || (read_loop_count-- > 0))
        {
          if (mems_read(&info, &data))
          {
            //convert_dataframe_to_string(raw80, &data.raw80, 28);
            //convert_dataframe_to_string(raw7d, &data.raw7d, 32);

            sprintf(log_line, "%s,"
                              "%d,%d,%d,%d,%d,%f,%f,%f,%d,%d,"
                              "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                              "%d,%d,%d,"
                              "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                              "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                              "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                              "80%s,7d%s\n",
                    simple_current_time(),
                    data.engine_rpm,
                    data.coolant_temp_c,
                    data.ambient_temp_c,
                    data.intake_air_temp_c,
                    data.fuel_temp_c,
                    data.map_kpa,
                    data.battery_voltage,
                    data.throttle_pot_voltage,
                    data.idle_switch,
                    data.uk1,
                    data.park_neutral_switch,
                    data.fault_codes,
                    data.idle_set_point,
                    data.idle_hot,
                    data.uk2,
                    data.iac_position,
                    data.idle_error,
                    data.ignition_advance_offset,
                    data.ignition_advance,
                    data.coil_time,
                    data.crankshaft_position_sensor,
                    data.uk4,
                    data.uk5,
                    data.ignition_switch,
                    data.throttle_angle,
                    data.uk6,
                    data.air_fuel_ratio,
                    data.dtc2,
                    data.lambda_voltage_mv,
                    data.lambda_sensor_frequency,
                    data.lambda_sensor_dutycycle,
                    data.lambda_sensor_status,
                    data.closed_loop,
                    data.long_term_fuel_trim,
                    data.short_term_fuel_trim,
                    data.carbon_canister_dutycycle,
                    data.dtc3,
                    data.idle_base_pos,
                    data.uk7,
                    data.dtc4,
                    data.ignition_advance2,
                    data.idle_speed_offset,
                    data.idle_error2,
                    (((uint16_t)data.idle_error2 << 8) | data.uk10),
                    data.dtc5,
                    data.uk11,
                    data.uk12,
                    data.uk13,
                    data.uk14,
                    data.uk15,
                    data.uk16,
                    data.uk1A,
                    data.uk1B,
                    data.uk1C,
                    data.raw7d,
                    data.raw80);
            printf("%s", log_line);
            syslog(LOG_NOTICE, "%s", log_line);

            // write to log file if enabled
            if (fp)
              write_log(&fp, log_line);

            // determine whether we need to split the output into manageable files
            // specify max size in Mb
            //
            // reading from MEMS at ~1 readings per second
            // 240Kb will record in 20 minute chunks
            config.output = split_log_file(&fp, 240000);

            // force a sleep of 450ms to get 2 readings per second
            sleep_ms(450);

            success = true;
          }
        }
        break;

      case MC_Read_Raw:
        while (read_inf || (read_loop_count-- > 0))
        {
          led(1);

          if (mems_read_raw(&info, &frame80, &frame7d))
          {
            frameptr = (uint8_t *)&frame80;
            printf("80: ");
            for (bufidx = 0; bufidx < sizeof(mems_data_frame_80); ++bufidx)
            {
              printf("%02X ", frameptr[bufidx]);
            }
            printf("\n");

            frameptr = (uint8_t *)&frame7d;
            printf("7D: ");
            for (bufidx = 0; bufidx < sizeof(mems_data_frame_7d); ++bufidx)
            {
              printf("%02X ", frameptr[bufidx]);
            }
            printf("\n");

            success = true;
          }

          led(0);
        }
        break;

      case MC_Read_IAC:
        if (mems_read_iac_position(&info, &readval))
        {
          printf("0x%02X\n", readval);
          syslog(LOG_NOTICE, "IAC position : 0x%02X", readval);
          success = true;
        }
        break;

      case MC_PTC:
        if (mems_test_actuator(&info, MEMS_PTCRelayOn, NULL))
        {
          sleep(2);
          success = mems_test_actuator(&info, MEMS_PTCRelayOff, NULL);
        }
        break;

      case MC_FuelPump:
        if (mems_test_actuator(&info, MEMS_FuelPumpOn, NULL))
        {
          sleep(2);
          success = mems_test_actuator(&info, MEMS_FuelPumpOff, NULL);
        }
        break;

      case MC_IAC_Close:
        do
        {
          success = mems_test_actuator(&info, MEMS_CloseIAC, &readval);

          // For some reason, diagnostic tools will continue to send send the
          // 'close' command many times after the IAC has already reached the
          // fully-closed position. Emulate that behavior here.
          if (success && (readval == 0x00))
          {
            iac_limit_count -= 1;
          }
        } while (success && iac_limit_count);
        break;

      case MC_IAC_Open:
        // The SP Rover 1 pod considers a value of 0xB4 to represent an opened
        // IAC valve, so repeat the open command until the valve is opened to
        // that point.
        do
        {
          success = mems_test_actuator(&info, MEMS_OpenIAC, &readval);
        } while (success && (readval < 0xB4));
        break;

      case MC_AC:
        if (mems_test_actuator(&info, MEMS_ACRelayOn, NULL))
        {
          sleep(2);
          success = mems_test_actuator(&info, MEMS_ACRelayOff, NULL);
        }
        break;

      case MC_Coil:
        success = mems_test_actuator(&info, MEMS_FireCoil, NULL);
        break;

      case MC_Injectors:
        success = mems_test_actuator(&info, MEMS_TestInjectors, NULL);
        break;

      case MC_Interactive:
        success = interactive_mode(&info, response_buffer);
        break;

      default:
        printf("Error: invalid command\n");
        syslog(LOG_ERR, "invalid command.");
        break;
      }
    }
    else
    {
      printf("Error in initialization sequence.\n");
      syslog(LOG_ERR, "Error in initialization sequence.");
    }
    mems_disconnect(&info);
  }
  else
  {
#if defined(WIN32)
    printf("Error: could not open serial device (%s).\n", win32devicename);
#else
    printf("Error: could not open serial device (%s).\n", argv[1]);
    syslog(LOG_ERR, "Error: could not open serial device (%s).\n", argv[1]);
#endif
  }

  mems_cleanup(&info);

  // close any open files
  if (fp)
  {
    // delete empty log files
    if (get_file_size(fp) < 1000)
    {
      printf("Output file too small, removing.\n");
      syslog(LOG_NOTICE, "Output file too small, removing.");
      delete_file(config.output);
    }

    fclose(fp);
  }

  closelog();

  led(0);

  return success
             ? 0
             : -2;
}
