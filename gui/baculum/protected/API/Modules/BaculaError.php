<?php
/*
 * Bacula(R) - The Network Backup Solution
 * Baculum   - Bacula web interface
 *
 * Copyright (C) 2013-2023 Kern Sibbald
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

use Baculum\Common\Modules\Errors\BaculaError as BError;

/**
 * Parse and handle Bacula errors.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Error
 * @package Baculum API
 */
class BaculaError extends APIModule {

	const DIRECTOR_ERROR_PATTERN = '/^\s*\[(?P<code>D[A-Z]\d{4})\]/';
	const STORAGE_ERROR_PATTERN = '/^\s*\[(?P<code>S[A-Z]\d{4})\]/';
	const CLIENT_ERROR_PATTERN = '/^\s*\[(?P<code>F[A-Z]\d{4})\]/';
	const CONSOLE_ERROR_PATTERN = '/^\s*\[(?P<code>C[A-Z]\d{4})\]/';

	/**
	 * Find first error in output.
	 *
	 * @param array $output bconsole output
	 * @return array with code, error code and message
	 */
	public function checkForErrors($output) {
		$code = null;
		$error = BError::ERROR_NO_ERRORS;
		$errmsg = BError::MSG_ERROR_NO_ERRORS;
		for ($i = 0; $i < count($output); $i++) {
			if (preg_match(self::DIRECTOR_ERROR_PATTERN, $output[$i], $match) === 1) {
				$code = $match['code'];
				$error = BError::ERROR_BACULA_DIRECTOR_ERROR;
				$errmsg = BError::MSG_ERROR_BACULA_DIRECTOR_ERROR;
				break;
			} elseif (preg_match(self::STORAGE_ERROR_PATTERN, $output[$i], $match) === 1) {
				$code = $match['code'];
				$error = BError::ERROR_BACULA_STORAGE_ERROR;
				$errmsg = BError::MSG_ERROR_BACULA_STORAGE_ERROR;
				break;
			} elseif (preg_match(self::CLIENT_ERROR_PATTERN, $output[$i], $match) === 1) {
				$code = $match['code'];
				$error = BError::ERROR_BACULA_CLIENT_ERROR;
				$errmsg = BError::MSG_ERROR_BACULA_CLIENT_ERROR;
				break;
			} elseif (preg_match(self::CONSOLE_ERROR_PATTERN, $output[$i], $match) === 1) {
				$code = $match['code'];
				$error = BError::ERROR_BACULA_CONSOLE_ERROR;
				$errmsg = BError::MSG_ERROR_BACULA_CONSOLE_ERROR;
				break;
			}
		}
		$result = [
			'code' => $code,
			'error' => $error,
			'errmsg' => $errmsg
		];
		return $result;
	}
}
