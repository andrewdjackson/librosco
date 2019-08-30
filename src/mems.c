#include <Python.h>
#include "rosco.h"

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

static char module_docstring[] =
    "Python Extension for Rover MEMS ECU library librosco 2.0.0";
static char mems_docstring[] =
    "Python Extension for Rover MEMS ECU library librosco 2.0.0";

static PyObject *get_version(PyObject *self, PyObject *args);
static PyObject *read_mems(PyObject *self, PyObject *args);
static PyObject *connect_mems(PyObject *self, PyObject *args);
static PyObject *interactive_command_mems(PyObject *self, PyObject *args);

char *simple_current_time(void);
int16_t readserial(mems_info *info, uint8_t *buffer, uint16_t quantity);
int16_t writeserial(mems_info *info, uint8_t *buffer, uint16_t quantity);
char *bytes_to_string(uint8_t *buf, unsigned int count);

static PyMethodDef module_methods[] = {
    {"version", get_version, METH_VARARGS, mems_docstring},
    {"read", read_mems, METH_VARARGS, mems_docstring},
    {"connect", connect_mems, METH_VARARGS, mems_docstring},
    {"command", interactive_command_mems, METH_VARARGS, mems_docstring},
    {NULL, NULL, 0, NULL}};

static mems_data data;
static mems_info info;
static bool connected;
static bool initialised;

PyMODINIT_FUNC PyInit_librosco(void)
{

   PyObject *module;
   static struct PyModuleDef moduledef = {
       PyModuleDef_HEAD_INIT,
       "librosco",
       module_docstring,
       -1,
       module_methods,
       NULL,
       NULL,
       NULL,
       NULL};
   module = PyModule_Create(&moduledef);
   if (!module)
      return NULL;

   return module;
}

static PyObject *get_version(PyObject *self, PyObject *args)
{
   char version[10];
   librosco_version ver;
   ver = mems_get_lib_version();

   sprintf(version, "%d.%d.%d", ver.major, ver.minor, ver.patch);

   /* Build the output  */
   PyObject *ret = Py_BuildValue("{ss}", "version", version);
   return ret;
}

static PyObject *interactive_command_mems(PyObject *self, PyObject *args)
{
   char *command;
   uint8_t icmd;
   ssize_t bytes_read = 0;
   ssize_t total_bytes_read = 0;
   char *response;
   uint8_t response_buffer[16384];

   // Parse the parameters
   if (!PyArg_ParseTuple(args, "s", &command))
      return NULL;

   if (initialised)
   {
      // convert command to a hex value
      icmd = strtoul(command, NULL, 16);

      if ((icmd >= 0) && (icmd <= 0xff))
      {
         if (writeserial(&info, &icmd, 1) == 1)
         {
            bytes_read = 0;
            total_bytes_read = 0;
            do
            {
               bytes_read = readserial(&info, response_buffer + total_bytes_read, 1);
               total_bytes_read += bytes_read;
            } while (bytes_read > 0);

            if (total_bytes_read > 0)
            {
               /* Build the output  */
               response = bytes_to_string(response_buffer, total_bytes_read);
               PyObject *ret = Py_BuildValue("{ssss}", "command", command, "response", response);
               return ret;
            }
            else
            {
               printf("No response from ECU.\n");
            }
         }
         else
         {
            printf("Error: failed to write command byte to serial port.\n");
         }
      }
   }

   Py_RETURN_NONE;
}

static PyObject *read_mems(PyObject *self, PyObject *args)
{
   if (initialised)
   {
      if (mems_read(&info, &data))
      {
         /* Build the output  */
         PyObject *ret = Py_BuildValue("{sssisisisisisfsfsfsisisisisisisisisisisisisisisisisisisisisisisisisisisisisisisisisisisisisisisisisisisisisisi}",
                                       "#time", simple_current_time(),
                                       "80x01-02_engine-rpm", data.engine_rpm,
                                       "80x03_coolant_temp", data.coolant_temp_c,
                                       "80x04_ambient_temp", data.ambient_temp_c,
                                       "80x05_intake_air_temp", data.intake_air_temp_c,
                                       "80x06_fuel_temp", data.fuel_temp_c,
                                       "80x07_map_kpa", data.map_kpa,
                                       "80x08_battery_voltage", data.battery_voltage,
                                       "80x09_throttle_pot", data.throttle_pot_voltage,
                                       "80x0A_idle_switch", data.idle_switch,
                                       "80x0B_uk1", data.uk1,
                                       "80x0C_park_neutral_switch", data.park_neutral_switch,
                                       "80x0D-0E_fault_codes", data.fault_codes,
                                       "80x0F_idle_set_point", data.idle_set_point,
                                       "80x10_idle_hot", data.idle_hot,
                                       "80x11_uk2", data.uk2,
                                       "80x12_iac_position", data.iac_position,
                                       "80x13-14_idle_error", data.idle_error,
                                       "80x15_ignition_advance_offset", data.ignition_advance_offset,
                                       "80x16_ignition_advance", data.ignition_advance,
                                       "80x17-18_coil_time", data.coil_time,
                                       "80x19_crankshaft_position_sensor", data.crankshaft_position_sensor,
                                       "80x1A_uk4", data.uk4,
                                       "80x1B_uk5", data.uk5,
                                       "7dx01_ignition_switch", data.ignition_switch,
                                       "7dx02_throttle_angle", data.throttle_angle,
                                       "7dx03_uk6", data.uk6,
                                       "7dx04_air_fuel_ratio", data.air_fuel_ratio,
                                       "7dx05_dtc2", data.dtc2,
                                       "7dx06_lambda_voltage", data.lambda_voltage_mv,
                                       "7dx07_lambda_sensor_frequency", data.lambda_sensor_frequency,
                                       "7dx08_lambda_sensor_dutycycle", data.lambda_sensor_dutycycle,
                                       "7dx09_lambda_sensor_status", data.lambda_sensor_status,
                                       "7dx0A_closed_loop", data.closed_loop,
                                       "7dx0B_long_term_fuel_trim", data.long_term_fuel_trim,
                                       "7dx0C_short_term_fuel_trim", data.short_term_fuel_trim,
                                       "7dx0D_carbon_canister_dutycycle", data.carbon_canister_dutycycle,
                                       "7dx0E_dtc3", data.dtc3,
                                       "7dx0F_idle_base_pos", data.idle_base_pos,
                                       "7dx10_uk7", data.uk7,
                                       "7dx11_dtc4", data.dtc4,
                                       "7dx12_ignition_advance2", data.ignition_advance2,
                                       "7dx13_idle_speed_offset", data.idle_speed_offset,
                                       "7dx14_idle_error2", data.idle_error2,
                                       "7dx14-15_uk10", (((uint16_t)data.idle_error2 << 8) | data.uk10),
                                       "7dx16_dtc5", data.dtc5,
                                       "7dx17_uk11", data.uk11,
                                       "7dx18_uk12", data.uk12,
                                       "7dx19_uk13", data.uk13,
                                       "7dx1A_uk14", data.uk14,
                                       "7dx1B_uk15", data.uk15,
                                       "7dx1C_uk16", data.uk16,
                                       "7dx1D_uk17", data.uk1A,
                                       "7dx1E_uk18", data.uk1B,
                                       "7dx1F_uk19", data.uk1C);
         return ret;
      }
   }

   Py_RETURN_NONE;
}

static PyObject *connect_mems(PyObject *self, PyObject *args)
{
   char *port;
   // this is twice as large as the micro's on-chip ROM, so it's probably sufficient
   uint8_t response_buffer[16384];

   /* Parse the parameters */
   if (!PyArg_ParseTuple(args, "s", &port))
      return NULL;

#if defined(WIN32)
   // correct for microsoft's legacy nonsense by prefixing with "\\.\"
   strcpy(win32devicename, "\\\\.\\");
   strncat(win32devicename, port, 16);
   port = win32devicename;
#endif

   mems_init(&info);
   connected = mems_connect(&info, port);

   if (connected)
   {
      initialised = mems_init_link(&info, response_buffer);
   }

   /* Build the output  */
   PyObject *ret = Py_BuildValue("{sssbsb}", "port", port, "connected", connected, "initialised", initialised);
   return ret;
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

char *simple_current_time(void)
{
   time_t t = time(NULL);
   struct tm tm = *localtime(&t);
   struct timeval tv;
   static char buffer[50];

   gettimeofday(&tv, NULL);

   sprintf(buffer, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
   return buffer;
}

char *bytes_to_string(uint8_t *buf, unsigned int count)
{
   static char buffer[150];
   unsigned int idx = 0;

   while (idx < count)
   {
      idx += 1;
      sprintf(buffer + strlen(buffer), "%02X", buf[idx - 1]);
   }

   return buffer;
}