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

use Baculum\API\Modules\BaculumAPIServer;
use Baculum\Common\Modules\Errors\GenericError;
use Baculum\Common\Modules\Params;

/**
 * Get application version.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category System
 * @package Baculum API
 */
class AppVersion extends BaculumAPIServer {

	public function get() {
		$this->output = [
			'version' => Params::BACULUM_VERSION
		];
		$this->error = GenericError::ERROR_NO_ERRORS;
	}
}
