<?php
/*
 * Bacula(R) - The Network Backup Solution
 * Baculum   - Bacula web interface
 *
 * Copyright (C) 2013-2024 Kern Sibbald
 *
 * The main author of Baculum is Marcin Haba.
 * The original author of Bacula is Kern Sibbald, with contributions
 * from many others, a complete list can be found in the file AUTHORS.
 *
 * You may use this file and others of this release according to the
 * license defined in the LICENSE file, which includes the Affero General
 * Public License, v3.0 ("AGPLv3") and some additional permissions and
 * terms pursuant to its AGPLv3 Section 7.
 *
 * This notice must be preserved when any source code is
 * conveyed and/or propagated.
 *
 * Bacula(R) is a registered trademark of Kern Sibbald.
 */

namespace Baculum\API\Modules;

use Baculum\Common\Modules\Errors\AWSToolError;
use Baculum\Common\Modules\Logging;

/**
 * Module responsible for managing AWS CLI tool.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Module
 * @package Baculum API
 */
class AWSCliTool extends APIModule {

	/**
	 * SUDO command.
	 */
	const SUDO = 'sudo';

	/**
	 * AWS CLI tool command pattern.
	 */
	const AWS_TOOL_COMMAND_PATTERN = 'LANG=C AWS_ACCESS_KEY_ID="%access_key" AWS_SECRET_ACCESS_KEY="%secret_key" %use_sudo%bin %options 2>&1';

	/**
	 * Check if AWS CLI tool is enabled.
	 *
	 * @return bool true if it is enabled, otherwise false
	 */
	private function isToolEnabled() {
		// temporary set staticaly to enabled
		return true;
	}

	/**
	 * Prepare AWS CLI tool results.
	 *
	 * @param mixed $output result output
	 * @param integer $error error number
	 * @return array output and error
	 */
	public function prepareResult($output, $error) {
		$result = [
			'output' => $output,
			'error' => $error
		];
		return $result;
	}

	/**
	 * Prepare and check AWS CLI tool output.
	 *
	 * @param array $output command output
	 * @return mixed array with results or null if output is unable to parse
	 */
	private function prepareOutput(array $output) {
		$output_txt = implode('', $output);
		$out = json_decode($output_txt, true);
		if (!is_array($out)) {
			$this->getModule('logging')->log(
				Logging::CATEGORY_EXTERNAL,
				"Parse output error: $output_txt"
			);
			$out = null;
		}
		return $out;
	}

	/**
	 * Get sudo command.
	 *
	 * @param bool $use_sudo true to use sudo, false otherwise
	 * @return string sudo command
	 */
	private function getSudo($use_sudo = false) {
		$sudo = '';
		if ($use_sudo === true) {
			$sudo = self::SUDO . ' ';
		}
		return $sudo;
	}

	/**
	 * Execute AWS CLI command.
	 *
	 * @param array $params command parameters
	 * @return mixed array with results or null if AWS CLI is disabled.
	 */
	public function execCommand($params = []) {
		$result = null;
		if ($this->isToolEnabled() === true) {
			$tool = $this->getTool();
			$result = $this->execTool(
				$tool['bin'],
				$tool['use_sudo'],
				$params
			);
			$output = $result['output'];
			$error = $result['error'];
			if ($error === 0) {
				$output = $this->prepareOutput($output);
				if (is_null($output)) {
					$output = AWSToolError::MSG_ERROR_AWS_TOOL_UNABLE_TO_PARSE_OUTPUT;
					$error = AWSToolError::ERROR_AWS_TOOL_UNABLE_TO_PARSE_OUTPUT;
				}
			} else {
				$emsg = PHP_EOL . ' Output:' . implode(PHP_EOL, $output);
				$output = AWSToolError::MSG_ERROR_WRONG_EXITCODE . $emsg;
				$error = AWSToolError::ERROR_WRONG_EXITCODE;
			}
			$result = $this->prepareResult($output, $error);
		} else {
			$output = AWSToolError::MSG_ERROR_AWS_TOOL_DISABLED;
			$error = AWSToolError::ERROR_AWS_TOOL_DISABLED;
			$result = $this->prepareResult($output, $error);
		}
		return $result;
	}

	/**
	 * Get AWS CLI tool binary.
	 *
	 * @return array AWS CLI binary parameters
	 */
	private function getTool() {
		/**
		 * Parameters set staticaly because of the temporary AWS CLI implementation.
		 */
		return [
			'bin' => 'aws',
			'cfg' => '',
			'use_sudo' => false
		];
	}

	/**
	 * Execute AWS CLI tool (internal method)
	 *
	 * @param string $bin tool binary path
	 * @param boolean use_sudo true to use sudo, false otherwise
	 * @return array output and exitcode results
	 */
	private function execTool($bin, $use_sudo, $params = []) {
		$sudo = $this->getSudo($use_sudo);
		$cmd = $this->getCmd($sudo, $bin, $params);
		exec($cmd, $output, $exitcode);
		$this->getModule('logging')->log(
			Logging::CATEGORY_EXECUTE,
			Logging::prepareOutput($cmd, $output)
		);
		$result = $this->prepareResult($output, $exitcode);
		return $result;
	}

	/**
	 * Get AWS CLi tool command.
	 *
	 * @param string $bin tool binary path
	 * @param bool $use_sudo use sudo with tool
	 * @param array $params command parameters
	 * @return string command
	 */
	private function getCmd($bin, $use_sudo, $params) {
		// Default command pattern
		$pattern = self::AWS_TOOL_COMMAND_PATTERN;
		$access_key = '';
		if (key_exists('access_key', $params)) {
			$access_key = $params['access_key'];
		}
		$secret_key = '';
		if (key_exists('secret_key', $params)) {
			$secret_key = $params['secret_key'];
		}
		$options = '';
		if (key_exists('options', $params) && is_array($params['options']) && count($params['options']) > 0) {
			$options = '"' . implode('" "', $params['options']) . '"';
		}
		$pattern = str_replace('%access_key', $access_key, $pattern);
		$pattern = str_replace('%secret_key', $secret_key, $pattern);
		$pattern = str_replace('%bin', $bin, $pattern);
		$pattern = str_replace('%use_sudo', $use_sudo, $pattern);
		$pattern = str_replace('%options', $options, $pattern);
		return $pattern;
	}
}
