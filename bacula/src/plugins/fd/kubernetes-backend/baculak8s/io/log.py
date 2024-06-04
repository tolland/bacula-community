# Bacula(R) - The Network Backup Solution
#
#   Copyright (C) 2000-2022 Kern Sibbald
#
#   The original author of Bacula is Kern Sibbald, with contributions
#   from many others, a complete list can be found in the file AUTHORS.
#
#   You may use this file and others of this release according to the
#   license defined in the LICENSE file, which includes the Affero General
#   Public License, v3.0 ("AGPLv3") and some additional permissions and
#   terms pursuant to its AGPLv3 Section 7.
#
#   This notice must be preserved when any source code is
#   conveyed and/or propagated.
#
#   Bacula(R) is a registered trademark of Kern Sibbald.

import logging
import os
import baculak8s

PLUGIN_WORKING = os.getenv("PLUGIN_WORKING", "/opt/bacula/working/kubernetes")
PRE_JOB_LOG_NAME_TEMPLATE = "pre_job_{}.log"
LOG_NAME_TEMPLATE = "{pid}_{job_id}_{job_name}.log"


class LogConfig(object):
    """
        Class used in order to configure the execution Debug Log File
        It determines whether the Debug Log File should be created, and
        where it should be created
    """

    @staticmethod
    def start():
        # The Log File should be at the PLUGIN_WORKING path
        if not os.path.exists(PLUGIN_WORKING):
            try:
                os.makedirs(PLUGIN_WORKING)
            except:
                # fallback to /tmp
                baculak8s.io.log.PLUGIN_WORKING = '/tmp/backendplugin'
                if not os.path.exists(PLUGIN_WORKING):
                    os.makedirs(PLUGIN_WORKING)

        # The Log File starts with a "Pre Job" name
        file_name = PRE_JOB_LOG_NAME_TEMPLATE.format(os.getpid())
        file_name = os.path.join(PLUGIN_WORKING, file_name)
        logging.basicConfig(filename=file_name, level=logging.DEBUG, filemode='w+', format='%(levelname)s:[%(pathname)s:%(lineno)d in %(funcName)s] %(message)s')

    @staticmethod
    def handle_params(job_info, plugin_params):
        if "debug" in plugin_params and plugin_params["debug"]:
            LogConfig._create(job_info)
            Log.debug_level = int(plugin_params["debug"])
        else:
            LogConfig._delete_pre_job_log()

    @staticmethod
    def _create(job_info):
        pid = os.getpid()
        old_name = PRE_JOB_LOG_NAME_TEMPLATE.format(pid)
        new_name = LOG_NAME_TEMPLATE.format(pid=pid, job_id=job_info["jobid"], job_name=job_info["name"])
        old_name = os.path.join(PLUGIN_WORKING, old_name)
        new_name = os.path.join(PLUGIN_WORKING, new_name)
        if os.path.isfile(old_name):
            os.rename(old_name, new_name)

    @staticmethod
    def _delete_pre_job_log():
        pid = os.getpid()
        pre_job_log_file = PRE_JOB_LOG_NAME_TEMPLATE.format(pid)
        pre_job_log_file = os.path.join(PLUGIN_WORKING, pre_job_log_file)
        if os.path.isfile(pre_job_log_file):
            os.remove(pre_job_log_file)


class Log:
    """
        Class with helper methods to send data to the Debug Log
    """

    debug_level = 0

    @staticmethod
    def save_received_termination(packet_header):
        Log.save_received_packet(packet_header, "(TERMINATION PACKET)")

    @staticmethod
    def save_received_eod(packet_header):
        Log.save_received_packet(packet_header, "(EOD PACKET)")

    @staticmethod
    def save_received_data(packet_header):
        Log.save_received_packet(packet_header, "(DATA PACKET)")

    @staticmethod
    def save_received_packet(packet_header, packet_content):
        if Log.debug_level <= 1:
            return
        if Log.debug_level == 2 and packet_header.decode()[0] == 'D':
            return
        message = "Received Packet\n{}\n{}\n".format(packet_header.decode(), packet_content)
        logging.debug(message)

    @staticmethod
    def save_sent_eod(packet_header):
        Log.save_sent_packet(packet_header, "(EOD PACKET)\n")

    @staticmethod
    def save_sent_data(packet_header):
        Log.save_sent_packet(packet_header, "(DATA PACKET)\n")

    @staticmethod
    def save_sent_packet(packet_header, packet_content):
        if Log.debug_level <= 1:
            return
        if Log.debug_level == 2 and packet_header[0] == 'D':
            return
        message = "Sent Packet\n{}{}".format(packet_header, packet_content)
        logging.debug(message)

    @staticmethod
    def save_exit_code(exit_code):
        message = "Backend finished with Exit Code: {}".format(exit_code)
        logging.debug(message)

    @staticmethod
    def save_exception(e):
        logging.exception(e)
