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

/**
 * Tools used to disable Bacula resources using disable bconsole command.
 * Note: this setting is not persistent. It is lost after the component restart.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Module
 * @package Baculum API
 */
class DisableResource extends APIModule {

	/**
	 * Resources allowed to disable.
	 * This resources support disabling/enabling.
	 */
	const ALLOWED_RESOURCES = [
		'dir' => [
			'Client',
			'Storage',
			'Job',
			'Schedule'
		],
		'sd' => [
			'Storage'
		],
		'fd' => [
			'FileDaemon'
		]
	];

	public function isResourceSupported($component_type, $resource_type) {
		return isset(self::ALLOWED_RESOURCES[$component_type][$resource_type]);
	}

	/**
	 * Disable resource.
	 *
	 * @param string $director Director name
	 * @param string $component_type component type
	 * @param string $resource_type resource type
	 * @param string $resource_name resource name
	 * @param array $params extra parameters
	 * @return object result and exitcode
	 */
	public function disable($director, $component_type, $resource_type, $resource_name, $params = []) {
		$ret = (object) ['output' => '','exitcode' => -1];
		if ($this->isResourceSupported($component_type, $resource_type)) {
			$ret->output = 'Resource not supported';
			return $ret;
		}

		$key = strtolower($resource_type);
		$value = $resource_name;
		$cmd = ['disable', "$key=\"$value\""];
		if (count($params) > 0) {
			$cmd = array_merge($cmd, $params);
		}
		$ret = $this->getModule('bconsole')->bconsoleCommand(
			$director,
			$cmd
		);
		return $ret;
	}
}
