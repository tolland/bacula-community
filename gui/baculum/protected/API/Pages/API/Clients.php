<?php
/*
 * Bacula(R) - The Network Backup Solution
 * Baculum   - Bacula web interface
 *
 * Copyright (C) 2013-2019 Kern Sibbald
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
use Baculum\API\Modules\ClientManager;
use Baculum\API\Modules\Database;
use Baculum\Common\Modules\Errors\ClientError;

/**
 * Clients endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class Clients extends BaculumAPIServer {

	public function get() {
		$misc = $this->getModule('misc');
		$client = $this->Request->contains('name') && $misc->isValidName($this->Request['name']) ? $this->Request['name'] : '';
		$limit = $this->Request->contains('limit') ? intval($this->Request['limit']) : 0;
		$offset = $this->Request->contains('offset') && $misc->isValidInteger($this->Request['offset']) ? (int)$this->Request['offset'] : 0;
		$plugin = $this->Request->contains('plugin') && $misc->isValidAlphaNumeric($this->Request['plugin']) ? $this->Request['plugin'] : '';
		$os = $this->Request->contains('os') && $misc->isValidNameExt($this->Request['os']) ? $this->Request['os'] : '';
		$version = $this->Request->contains('version') && $misc->isValidColumn($this->Request['version']) ? $this->Request['version'] : '';
		$type = $this->Request->contains('type') && $misc->isValidName($this->Request['type']) ? $this->Request['type'] : null;
		$mode = $this->Request->contains('overview') && $misc->isValidBooleanTrue($this->Request['overview']) ? ClientManager::CLIENT_RESULT_MODE_OVERVIEW : ClientManager::CLIENT_RESULT_MODE_NORMAL;
		$order_by = $this->Request->contains('order_by') && $misc->isValidColumn($this->Request['order_by']) ? $this->Request['order_by']: null;
		$order_direction = $this->Request->contains('order_direction') && $misc->isValidOrderDirection($this->Request['order_direction']) ? $this->Request['order_direction']: null;
		$result = $this->getModule('bconsole')->bconsoleCommand($this->director, array('.client'));
		if ($result->exitcode === 0) {
			$params = [];

			$clients = [];
			if (!empty($client)) {
				if (in_array($client, $result->output)) {
					$clients = [$client];
				} else {
					// no client name provided but not found in the configuration
					$this->output = ClientError::MSG_ERROR_CLIENT_DOES_NOT_EXISTS;
					$this->error = ClientError::ERROR_CLIENT_DOES_NOT_EXISTS;
					return;
				}
			} else {
				$clients = $result->output;
			}
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

			if (!empty($plugin)) {
				$params['Plugins'] = [];
				$params['Plugins'][] = [
					'operator' => 'LIKE',
					'vals' => "%{$plugin}%"
				];
			}

			$db_params = $this->getModule('api_config')->getConfig('db');
			$regex_op = $db_params['type'] == Database::PGSQL_TYPE ? '~*' : 'REGEXP';
			if (!empty($os) || !empty($version)) {
				$params['Client.Uname'] = [];
				if (!empty($os)) {
					$params['Client.Uname'][] = [
						'operator' => $regex_op,
						'vals' => '^[[:alnum:]]+\\.[[:alnum:]]+\\.[[:alnum:]]+ +\\([[:alnum:]]+\\) +.*' . $os . '.*$'
					];
				}
				if (!empty($version)) {
					$params['Client.Uname'][] = [
						'operator' => $regex_op,
						'vals' => '^.*' . $version . '.* +\\([[:alnum:]]+\\) *.*$'
					];
				}
			}

			$jobs = [];
			$result = $this->getModule('bconsole')->bconsoleCommand(
				$this->director,
				['.jobs'],
				null,
				true
			);
			if ($result->exitcode === 0) {
				$jobs = $result->output;
			}

			$sort = [];
			if (!is_null($order_by)) {
				if (is_null($order_direction)) {
					$order_direction = 'ASC';
				}
				$cr = new \ReflectionClass('Baculum\API\Modules\ClientRecord');
				$sort_cols = $cr->getProperties();
				$order_by_lc = strtolower($order_by);
				$columns = [];
				foreach ($sort_cols as $cols) {
					$columns[] = $cols->getName();
				}
				if (!in_array($order_by_lc, $columns)) {
					$this->output = ClientError::MSG_ERROR_INVALID_PROPERTY;
					$this->error = ClientError::ERROR_INVALID_PROPERTY;
					return;
				}
				$sort = [[$order_by_lc, $order_direction]];
			}

			$clients = [];
			if ($mode == ClientManager::CLIENT_RESULT_MODE_OVERVIEW) {
				if (!in_array('Client.Uname', $params)) {
					$params['Client.Uname'] = [];
				}

				// reachable clients (uname exists)
				$params['Client.Uname'][] = [
					'operator' => '!=',
					'vals' => ''
				];
				$clients_reached = $this->getModule('client')->getClients(
					(is_null($type) || $type === ClientManager::CLIENT_TYPE_REACHABLE ? $limit : 0),
					(is_null($type) || $type === ClientManager::CLIENT_TYPE_REACHABLE ? $offset : 0),
					$sort,
					$params,
					$jobs,
					$mode
				);

				// unreachable clients (uname not exists)
				array_pop($params['Client.Uname']);
				$params['Client.Uname'][] = [
					'operator' => '=',
					'vals' => ''
				];
				$clients_unreached = $this->getModule('client')->getClients(
					(is_null($type) || $type === ClientManager::CLIENT_TYPE_UNREACHABLE ? $limit : 0),
					(is_null($type) || $type === ClientManager::CLIENT_TYPE_UNREACHABLE ? $offset : 0),
					$sort,
					$params,
					$jobs,
					$mode
				);

				if ($type === ClientManager::CLIENT_TYPE_REACHABLE) {
					$clients_unreached['clients'] = [];
				} elseif ($type === ClientManager::CLIENT_TYPE_UNREACHABLE) {
					$clients_reached['clients'] = [];
				}

				$clients = [
					'reachable' => $clients_reached,
					'unreachable' => $clients_unreached
				];
			} else {
				$clients = $this->getModule('client')->getClients(
					$limit,
					$offset,
					$sort,
					$params,
					$jobs,
					$mode
				);
			}
			$this->output = $clients;
			$this->error = ClientError::ERROR_NO_ERRORS;
		} else {

			$this->output = $result->output;
			$this->error = $result->exitcode;
		}
	}
}

?>
