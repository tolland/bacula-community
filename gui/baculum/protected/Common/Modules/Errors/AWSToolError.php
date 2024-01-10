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

namespace Baculum\Common\Modules\Errors;

/**
 * AWS tool error class.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Errors
 * @package Baculum Common
 */
class AWSToolError extends GenericError {

	const ERROR_AWS_TOOL_DISABLED = 540;
	const ERROR_AWS_MISSING_ACCESS_KEY = 541;
	const ERROR_AWS_MISSING_SECRET_KEY = 542;
	const ERROR_AWS_TOOL_UNABLE_TO_PARSE_OUTPUT = 543;

	const MSG_ERROR_AWS_TOOL_DISABLED = 'AWS tool support is disabled.';
	const MSG_ERROR_AWS_MISSING_ACCESS_KEY = 'Missing access key.';
	const MSG_ERROR_AWS_MISSING_SECRET_KEY = 'Missing secret key.';
	const MSG_ERROR_AWS_TOOL_UNABLE_TO_PARSE_OUTPUT = 'Unable to parse output.';
}
