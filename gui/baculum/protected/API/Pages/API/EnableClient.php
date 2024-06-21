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
use Baculum\Common\Modules\Errors\ClientError;

/**
 * Enable Director Client resource endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class EnableClient extends BaculumAPIServer {

	public function set($id, $params) {
		$clientid = (int) $id;
		$client_mod = $this->getModule('client');
		$client = null;
		if ($clientid > 0) {
			$client = $client_mod->getClientById($clientid);
		}

		$result = $this->getModule('bconsole')->bconsoleCommand(
			$this->director,
			['.client'],
			null,
			true
		);
		if ($result->exitcode === 0) {
			if (is_object($client) && in_array($client->name, $result->output)) {
				$component_type = 'dir';
				$resource_type = 'client';
				$resource_name = $client->name;

				$result = $this->getModule('enable')->enable(
					$this->director,
					$component_type,
					$resource_type,
					$resource_name
				);
				$output = $result->output;
				$error = $result->exitcode;
				if ($error != 0) {
					$output = ClientError::MSG_ERROR_WRONG_EXITCODE . 'Exitcode=>' . $result->exitcode. ', Output=>' . print_r($result->output, true);
					$error = ClientError::ERROR_WRONG_EXITCODE;
				}
				$this->output = $output;
				$this->error = $error;
			} else {
				$this->output = ClientError::MSG_ERROR_CLIENT_DOES_NOT_EXISTS;
				$this->error = ClientError::ERROR_CLIENT_DOES_NOT_EXISTS;
			}
		} else {
			$this->output = ClientError::MSG_ERROR_WRONG_EXITCODE . 'Exitcode=>' . $result->exitcode. ', Output=>' . print_r($result->output, true);
			$this->error = ClientError::ERROR_WRONG_EXITCODE;
		}
	}
}
?>
