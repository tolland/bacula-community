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

use Baculum\API\Modules\BaculumAPIServer;
use Baculum\Common\Modules\Errors\ClientError;

/**
 * Plugin names endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class ClientPluginNames extends BaculumAPIServer {
	public function get() {
		$misc = $this->getModule('misc');
		$extended = $this->Request->contains('extended') && $misc->isValidBoolean($this->Request['extended']) ? (bool)$this->Request['extended'] : false;
		$result = $this->getModule('bconsole')->bconsoleCommand(
			$this->director,
			['.client'],
			null,
			true
		);
		if ($result->exitcode === 0) {
			$params = [];
			$clients = array_filter($result->output);
			if (count($clients) == 0) {
				// no $clients criteria means that user has no client resource assigned.
				$this->output = [];
				$this->error = ClientError::ERROR_NO_ERRORS;
				return;
			}

			$params['Client.Name'] = [];
			$params['Client.Name'][] = [
				'operator' => 'IN',
				'vals' => $clients
			];

			$this->output = $this->getModule('client')->getClientsPlugins($params, $extended);
			$this->error = ClientError::ERROR_NO_ERRORS;
		} else {

			$this->output = $result->output . ', Exitcode=>' . $result->exitcode;
			$this->error = ClientError::ERROR_WRONG_EXITCODE;
		}

	}
}
