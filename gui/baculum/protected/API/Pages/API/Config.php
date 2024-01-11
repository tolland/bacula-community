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

use Baculum\API\Modules\BaculaConfig;
use Baculum\API\Modules\BaculumAPIServer;
use Baculum\Common\Modules\Errors\AuthorizationError;
use Baculum\Common\Modules\Errors\BaculaConfigError;

/**
 * Config endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class Config extends BaculumAPIServer {

	public function get() {
		$misc = $this->getModule('misc');
		$component_type = $this->Request->contains('component_type') ? $this->Request['component_type'] : null;
		$resource_type = $this->Request->contains('resource_type') ? $this->Request['resource_type'] : null;
		$resource_name = $this->Request->contains('resource_name') ? $this->Request['resource_name'] : null;
		$apply_jobdefs = $this->Request->contains('apply_jobdefs') && $misc->isValidBoolean($this->Request['apply_jobdefs']) ? (bool)$this->Request['apply_jobdefs'] : null;
		$filter_directive = $this->Request->contains('filter_directive') && $misc->isValidName($this->Request['filter_directive']) ? $this->Request['filter_directive'] : null;
		$filter_value = $this->Request->contains('filter_value') && $this->Request['filter_value'] ? $this->Request['filter_value'] : null;
		$opts = [];
		if ($apply_jobdefs) {
			$opts['apply_jobdefs'] = $apply_jobdefs;
		}
		if (!$this->isResourceAllowed(
			'READ',
			$component_type,
			$resource_type,
			$resource_name
		)) {
			// Access denied. End.
			return;
		}

		// Role valid. Access granted
		$config = $this->getModule('bacula_setting')->getConfig(
			$component_type,
			$resource_type,
			$resource_name,
			$opts
		);
		if ($config['exitcode'] === 0 && count($config['output']) == 0) {
			// Config does not exists. Nothing to get.
			$this->output = BaculaConfigError::MSG_ERROR_CONFIG_DOES_NOT_EXIST;
			$this->error = BaculaConfigError::ERROR_CONFIG_DOES_NOT_EXIST;
		} else {
			if (is_string($filter_directive) && is_string($filter_value)) {
				$config['output'] = BaculaConfig::filterResources(
					$config['output'],
					$filter_directive,
					$filter_value
				);
			}
			$this->output = $config['output'];
			$this->error = $config['exitcode'];
		}
	}

	public function set($id, $params) {
		$config = (array)$params;
		if (array_key_exists('config', $config)) {
			if ($this->getClientVersion() <= 0.2) {
				// old way sending config as serialized array
				$config = unserialize($config['config']);
			} else {
				$config = json_decode($config['config'], true);
			}
		} else {
			$config = [];
		}
		if (is_null($config)) {
			// Invalid config. End.
			$this->output = BaculaConfigError::MSG_ERROR_CONFIG_VALIDATION_ERROR;
			$this->error = BaculaConfigError::ERROR_CONFIG_VALIDATION_ERROR;
			return;
		}
		$component_type = $this->Request->contains('component_type') ? $this->Request['component_type'] : null;
		$resource_type = $this->Request->contains('resource_type') ? $this->Request['resource_type'] : null;
		$resource_name = $this->Request->contains('resource_name') ? $this->Request['resource_name'] : null;

		if (!$this->isResourceAllowed(
			'UPDATE',
			$component_type,
			$resource_type,
			$resource_name
		)) {
			// Access denied. End.
			return;
		}

		$resource_config = [];
		if (is_string($component_type) && is_string($resource_type) && is_string($resource_name)) {
			// Get existing resource config.
			$res = $this->getModule('bacula_setting')->getConfig(
				$component_type,
				$resource_type,
				$resource_name
			);
			if ($res['exitcode'] === 0) {
				$resource_config = $res['output'];
			}
		}

		if (is_null($resource_name) || count($resource_config) > 0 || $this->getModule('api_config')->isExtendedMode() === false) {
			// Config exists. Update it.
			$result = $this->getModule('bacula_setting')->setConfig(
				$config,
				$component_type,
				$resource_type,
				$resource_name
			);
			if ($result['save_result'] === true) {
				$this->output = BaculaConfigError::MSG_ERROR_NO_ERRORS;
				$this->error = BaculaConfigError::ERROR_NO_ERRORS;
			} else if ($result['is_valid'] === false) {
				$this->output = BaculaConfigError::MSG_ERROR_CONFIG_VALIDATION_ERROR . print_r($result['result'], true);
				$this->error = BaculaConfigError::ERROR_CONFIG_VALIDATION_ERROR;
			} else {
				$this->output = BaculaConfigError::MSG_ERROR_WRITE_TO_CONFIG_ERROR . print_r($result['result'], true);
				$this->error = BaculaConfigError::ERROR_WRITE_TO_CONFIG_ERROR;
			}
		} else {
			// Config does not exists. Nothing to update.
			$this->output = BaculaConfigError::MSG_ERROR_CONFIG_DOES_NOT_EXIST;
			$this->error = BaculaConfigError::ERROR_CONFIG_DOES_NOT_EXIST;
		}
	}

	public function create($params) {
		$config = (array)$params;
		$config = json_decode($config['config'], true);
		if (is_null($config)) {
			// Invalid config. End.
			$this->output = BaculaConfigError::MSG_ERROR_CONFIG_VALIDATION_ERROR;
			$this->error = BaculaConfigError::ERROR_CONFIG_VALIDATION_ERROR;
			return;
		}

		$component_type = $this->Request->contains('component_type') ? $this->Request['component_type'] : null;
		$resource_type = $this->Request->contains('resource_type') ? $this->Request['resource_type'] : null;
		$resource_name = $this->Request->contains('resource_name') ? $this->Request['resource_name'] : null;

		if (!$this->isResourceAllowed(
			'CREATE',
			$component_type,
			$resource_type,
			$resource_name
		)) {
			// Access denied. End.
			return;
		}

		$resource_config = [];
		if (is_string($component_type) && is_string($resource_type) && is_string($resource_name)) {
			// Get existing resource config.
			$res = $this->getModule('bacula_setting')->getConfig(
				$component_type,
				$resource_type,
				$resource_name
			);
			if ($res['exitcode'] === 0) {
				$resource_config = $res['output'];
			}
		}

		if (is_null($resource_name) || count($resource_config) == 0) {
			// Resource does not exists, so add it to config.
			$result = $this->getModule('bacula_setting')->setConfig(
				$config,
				$component_type,
				$resource_type,
				$resource_name
			);
			if ($result['save_result'] === true) {
				$this->output = BaculaConfigError::MSG_ERROR_NO_ERRORS;
				$this->error = BaculaConfigError::ERROR_NO_ERRORS;
			} else if ($result['is_valid'] === false) {
				$this->output = BaculaConfigError::MSG_ERROR_CONFIG_VALIDATION_ERROR . print_r($result['result'], true);
				$this->error = BaculaConfigError::ERROR_CONFIG_VALIDATION_ERROR;
			} else {
				$this->output = BaculaConfigError::MSG_ERROR_WRITE_TO_CONFIG_ERROR . print_r($result['result'], true);
				$this->error = BaculaConfigError::ERROR_WRITE_TO_CONFIG_ERROR;
			}
		} else {
			// Resource already exists. End.
			$this->output = BaculaConfigError::MSG_ERROR_CONFIG_ALREADY_EXISTS;
			$this->error = BaculaConfigError::ERROR_CONFIG_ALREADY_EXISTS;
		}
	}

	public function remove($id) {
		$component_type = $this->Request->contains('component_type') ? $this->Request['component_type'] : null;
		$resource_type = $this->Request->contains('resource_type') ? $this->Request['resource_type'] : null;
		$resource_name = $this->Request->contains('resource_name') ? $this->Request['resource_name'] : null;

		if (!$this->isResourceAllowed(
			'DELETE',
			$component_type,
			$resource_type,
			$resource_name
		)) {
			// Access denied. End.
			return;
		}

		$config = [];
		if (is_string($component_type) && is_string($resource_type) && is_string($resource_name)) {
			$res = $this->getModule('bacula_setting')->getConfig(
				$component_type
			);
			if ($res['exitcode'] === 0) {
				$config = $res['output'];
			}
		}
		$config_len = count($config);
		if ($config_len > 0) {
			$index_del = -1;
			for ($i = 0; $i < $config_len; $i++) {
				if (!key_exists($resource_type, $config[$i])) {
					// skip other resource types
					continue;
				}
				if ($config[$i][$resource_type]['Name'] === $resource_name) {
					$index_del = $i;
					break;
				}
			}
			if ($index_del > -1) {
				array_splice($config, $index_del, 1);
				$result = $this->getModule('bacula_setting')->setConfig(
					$config,
					$component_type
				);
				if ($result['save_result'] === true) {
					$this->output = BaculaConfigError::MSG_ERROR_NO_ERRORS;
					$this->error = BaculaConfigError::ERROR_NO_ERRORS;
				} else if ($result['is_valid'] === false) {
					$this->output = BaculaConfigError::MSG_ERROR_CONFIG_VALIDATION_ERROR . print_r($result['result'], true);
					$this->error = BaculaConfigError::ERROR_CONFIG_VALIDATION_ERROR;
				} else {
					$this->output = BaculaConfigError::MSG_ERROR_WRITE_TO_CONFIG_ERROR . print_r($result['result'], true);
					$this->error = BaculaConfigError::ERROR_WRITE_TO_CONFIG_ERROR;
				}
			} else {
				$this->output = BaculaConfigError::MSG_ERROR_CONFIG_DOES_NOT_EXIST;
				$this->error = BaculaConfigError::ERROR_CONFIG_DOES_NOT_EXIST;
			}
		} else {
			$this->output = BaculaConfigError::MSG_ERROR_CONFIG_DOES_NOT_EXIST;
			$this->error = BaculaConfigError::ERROR_CONFIG_DOES_NOT_EXIST;
		}
	}

	/**
	 * Access denied error.
	 * User is not allowed to use given action.
	 *
	 * @param string $component_type component type
	 * @param string $resource_type resource type
	 * @param string $resource_name resource name
	 * @return none
	 */
	private function accessDenied($component_type, $resource_type, $resource_name)
	{
		$emsg =  sprintf(
			' ComponentType: %s, ResourceType: %s, ResourceName: %s',
			$component_type,
			$resource_type,
			$resource_name
		);
		$this->output = AuthorizationError::MSG_ERROR_ACCESS_ATTEMPT_TO_NOT_ALLOWED_RESOURCE . $emsg;
		$this->error = AuthorizationError::ERROR_ACCESS_ATTEMPT_TO_NOT_ALLOWED_RESOURCE;
	}

	/**
	 * Check if resource is allowed for current user.
	 *
	 * @param string $component_type component type
	 * @param string $resource_type resource type
	 * @param string $resource_name resource name
	 * @return bool true if user is allowed, false otherwise
	 */
	private function isResourceAllowed($action, $component_type, $resource_type, $resource_name)
	{
		if ($this->getModule('api_config')->isExtendedMode() === false) {
			// without extended mode all resources are allowed
			return true;
		}
		$bacula_config_acl = $this->getModule('bacula_config_acl');
		$valid = $bacula_config_acl->validateCommand(
			$this->username,
			$action,
			$component_type,
			$resource_type
		);

		if (!$valid) {
			// Access denied. End.
			$this->accessDenied(
				$component_type,
				$resource_type,
				$resource_name
			);
		}
		return $valid;
	}
}
