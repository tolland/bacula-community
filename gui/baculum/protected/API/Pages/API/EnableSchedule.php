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

/**
 * Enable Director Schedule resource endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class EnableSchedule extends BaculumAPIServer {

	public function set($id, $params) {
		$misc = $this->getModule('misc');
		$schedule_name = $this->Request->contains('name') && $misc->isValidName($this->Request['name']) ? $this->Request['name'] : '';

		$result = $this->getModule('bconsole')->bconsoleCommand(
			$this->director,
			['.schedule'],
			null,
			true
		);
		if ($result->exitcode === 0) {
			if (in_array($schedule_name, $result->output)) {
				$component_type = 'dir';
				$resource_type = 'schedule';
				$resource_name = $schedule_name;

				$result = $this->getModule('enable')->enable(
					$this->director,
					$component_type,
					$resource_type,
					$resource_name
				);
				$output = $result->output;
				$error = $result->exitcode;
				if ($error != 0) {
					$output = GenericError::MSG_ERROR_WRONG_EXITCODE . 'Exitcode=>' . $result->exitcode. ', Output=>' . print_r($result->output, true);
					$error = GenericError::ERROR_WRONG_EXITCODE;
				}
				$this->output = $output;
				$this->error = $error;
			} else {
				$this->output = GenericError::MSG_ERROR_INVALID_PROPERTY;
				$this->error = GenericError::ERROR_INVALID_PROPERTY;
			}
		} else {
			$this->output = GenericError::MSG_ERROR_WRONG_EXITCODE . 'Exitcode=>' . $result->exitcode. ', Output=>' . print_r($result->output, true);
			$this->error = GenericError::ERROR_WRONG_EXITCODE;
		}
	}
}
?>
