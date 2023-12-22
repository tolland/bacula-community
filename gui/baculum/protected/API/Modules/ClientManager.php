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

namespace Baculum\API\Modules;

use Baculum\API\Modules\Database;
use PDO;

/**
 * Client manager module.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Module
 * @package Baculum API
 */
class ClientManager extends APIModule {


	/**
	 * Client types
	 */
	const CLIENT_TYPE_REACHABLE = 'reachable';
	const CLIENT_TYPE_UNREACHABLE = 'unreachable';

	/**
	 * Result modes.
	 */
	const CLIENT_RESULT_MODE_NORMAL = 'normal';
	const CLIENT_RESULT_MODE_OVERVIEW = 'overview';

	/**
	 * Uname pattern to split values.
	 */
	const CLIENT_UNAME_PATTERN = '/^(?P<version>[\w\d\.]+)\s+\((?P<release_date>[\da-z]+\))?\s+(?P<os>.+)$/i';

	/**
	 * Get client list.
	 *
	 * @param mixed $limit_val result limit value
	 * @param int $offset_val result offset value
	 * @param array $sort order by and order direction in form [[by1, direction1]]
	 * @param array $criteria SQL criteria to get job list
	 * @param array $jobs jobs used to get job properties per client
	 * @param string $mode result mode
	 * @return array clients or empty list if no client found
	 */
	public function getClients($limit_val = 0, $offset_val = 0, $sort = [['Name', 'ASC']], $criteria = [], $jobs = [], $mode = self::CLIENT_RESULT_MODE_NORMAL) {
		$extra_cols = ['os', 'version'];
		$order = '';
		if (count($sort) == 1 && !in_array($sort[0][0], $extra_cols)) {
			$order = Database::getOrder($sort);
		}
		$limit = '';
		if(is_int($limit_val) && $limit_val > 0) {
			$limit = ' LIMIT ' . $limit_val;
		}
		$offset = '';
		if (is_int($offset_val) && $offset_val > 0) {
			$offset = ' OFFSET ' . $offset_val;
		}
		$jcriteria = [];
		if (count($jobs) > 0) {
			$jcriteria['Job.Name'] = [];
			$jcriteria['Job.Name'][] = [
				'operator' => 'IN',
				'vals' => $jobs
			];
		}
		$jwhere = Database::getWhere($jcriteria, true);
		if (!empty($jwhere['where'])) {
			$jwhere['where'] = ' AND ' . $jwhere['where'];
		}
		$where = Database::getWhere($criteria);

		$sql = '
SELECT DISTINCT 
	Client.*,
	COALESCE(J.running_jobs, 0) AS running_jobs
FROM
	Client AS Client
	LEFT JOIN (
		SELECT ClientId AS clientid, COUNT(1) AS running_jobs
		FROM Client
		JOIN Job USING (ClientId)
		WHERE Job.JobStatus IN (\'C\', \'R\')
		' . $jwhere['where'] . '
		GROUP BY ClientId
	) AS J ON (Client.ClientId = J.ClientId) 
	' . $where['where'] . $order . $offset . $limit;
		$params = array_merge($jwhere['params'], $where['params']);
		$statement = Database::runQuery($sql, $params);
		$result = $statement->fetchAll(\PDO::FETCH_ASSOC);
		$this->setSpecialFields($result);
		if (count($sort) == 1 && in_array($sort[0][0], $extra_cols)) {
			$misc = $this->getModule('misc');
			// Order by os or version
			$misc::sortResultsByField(
				$result,
				$sort[0][0],
				$sort[0][1],
				'name',
				$misc::ORDER_DIRECTION_ASC
			);
		}
		if ($mode == self::CLIENT_RESULT_MODE_OVERVIEW) {
			$sql = 'SELECT COUNT(1) AS count FROM Client ' . $where['where'];
			$statement = Database::runQuery($sql, $where['params']);
			$count = $statement->fetch(\PDO::FETCH_OBJ);
			if (!is_object($count)) {
				$count = (object)['count' => 0];
			}
			$result = [
				'clients' => $result,
				'count' => $count->count
			];
		}
		return $result;
	}

	/**
	 * Set special client properties.
	 *
	 * @param array $result client results (note: reference)
	 */
	private function setSpecialFields(&$result) {
		for ($i = 0; $i < count($result); $i++) {
			// Add operating system info and client version
			$result[$i]['os'] = '';
			$result[$i]['version'] = '';
			if (preg_match(self::CLIENT_UNAME_PATTERN, $result[$i]['uname'], $match) === 1) {
				$result[$i]['os'] = $match['os'];
				$result[$i]['version'] = $match['version'];
			}
		}
	}

	public function getClientByName($name) {
		$result = ClientRecord::finder()->findByName($name);
		if (is_object($result)) {
			$result = [(array)$result];
			$this->setSpecialFields($result);
			$result = (object)array_shift($result);
		}
		return $result;
	}

	public function getClientById($id) {
		$result = ClientRecord::finder()->findByclientid($id);
		if (is_object($result)) {
			$result = [(array)$result];
			$this->setSpecialFields($result);
			$result = (object)array_shift($result);
		}
		return $result;
	}

	/**
	 * Get unique client plugin list from all clients for given director.
	 *
	 * @param array $criteria SQL criteria to get job list
	 * @param bool $extended extended mode, if true it returns plugin versions too
	 * @return array unique client plugin list
	 */
	public function getClientsPlugins($criteria, $extended = false) {
		$plugins = [];
		$params['Client.Plugins'] = [];
		$params['Client.Plugins'][] = [
			'operator' => '!=',
			'vals' => ''
		];
		$where = Database::getWhere($criteria);
		$db_params = $this->getModule('api_config')->getConfig('db');
		if ($db_params['type'] === Database::PGSQL_TYPE) {
			$sql = '
				SELECT
					DISTINCT regexp_split_to_table(Plugins, \',\') AS plugins
				FROM
					Client
				' . $where['where'];
			$statement = Database::runQuery($sql, $where['params']);
			$result = $statement->fetchAll(PDO::FETCH_COLUMN);
			$plugins = $result;
		} elseif ($db_params['type'] === Database::MYSQL_TYPE) {
			$sql = '
				SELECT
					Plugins AS plugins
				FROM
					Client
					' . $where['where'];
			$statement = Database::runQuery($sql, $where['params']);
			$result = $statement->fetchAll(PDO::FETCH_OBJ);
			for ($i = 0; $i < count($result); $i++) {
				$spl = explode(',', $result[$i]->plugins);
				$plugins = array_merge($plugins, $spl);
			}
		}
		$items = [];
		for ($i = 0; $i < count($plugins); $i++) {
			if (preg_match('/^(?P<plugin>[\w\-]+)\((?P<version>[\d\.]+)\)$/', $plugins[$i], $match) === 1) {
				if ($extended) {
					$items[] = [
						'plugin' => $match['plugin'],
						'version' => $match['version']
					];
				} else {
					$items[$match['plugin']] = $match['plugin'];
				}
			}
		}
		if (!$extended) {
			$items = array_values($items);
			sort($items);
		}
		return $items;
	}
}
?>
