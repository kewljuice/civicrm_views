<?php

/**
 * @file
 * Contains views-related code.
 */

use Drupal\Core\Database\Database;

/**
 * Implements hook_views_data().
 */
function civicrm_views_views_data() {
  $data = [];

  try {
    \Drupal::service('civicrm')->initialize();
  }
  catch (Exception $e) {
    \Drupal::logger('civicrm_views')
      ->error("Unable to initialize CiviCRM. Aborting hook_views_data().");
    return $data;
  }

  // Attempt to use 'civicrm' named database. Otherwise default to using default
  // Drupal database.
  // @Todo: linking to the Drupal user table doesn't actually work when using a
  // separate civicrm database.
  // We need to include civicrm.install so that we can use
  // _civicrm_get_db_config().
  \Drupal::moduleHandler()->loadInclude('civicrm', 'install');
  $database = _civicrm_get_db_config()['key'];

  // Use CiviCRM schema data to automatically populate $data array with fields
  // and make educated guesses for field, sort and filter plugins.
  $civicrm_entities = _civicrm_views_civicrm_entities();
  foreach ($civicrm_entities as $table => $civicrm_entity) {
    $data[$table]['table']['group'] = $civicrm_entity['display_name'];

    // Only define the base key if we intend for the current CiviCRM table to
    // be a base table in views (and not just a table that is brought in via a
    // relationship or an implicit join) ie. a table that you can start building
    // a view from.
    if ($civicrm_entity['base']) {
      $data[$table]['table']['base'] = [
        'field' => 'id',
        'title' => $civicrm_entity['display_name'],
        'database' => $database,
      ];
    };

    foreach ($civicrm_entity['dao']::fields() as $field) {
      $data[$table][$field['name']] = [
        'title' => !empty($field['title']) ? $field['title'] : "{$civicrm_entity['display_name']}.{$field['name']}",
        'help' => !empty($field['title']) ? $field['title'] : "{$civicrm_entity['display_name']}.{$field['name']}",
        'field' => _civicrm_views_field_to_field_plugin($civicrm_entity, $field),
        'sort' => _civicrm_views_field_to_sort_plugin($civicrm_entity, $field),
        'filter' => _civicrm_views_field_to_filter_plugin($civicrm_entity, $field),
        'argument' => _civicrm_views_field_to_argument_plugin($civicrm_entity, $field),
        'relationship' => [],
      ];
    }
  }

  _civicrm_views_add_relationships($data);
  _civicrm_views_add_implicit_joins($data);

  // Now load any custom tables.
  // We attach fields via their 'custom_n' notation to avoid accidentally
  // clobbering any core fields by the same name.
  foreach (_civicrm_views_get_custom_fields() as $field) {
    $data[$field['table']]['table']['group'] = "CiviCRM {$field['group']}";

    $data[$field['join']]["id_{$field['group']}"] = [
      'relationship' => [
        'id' => 'standard',
        'field' => 'id',
        'base' => $field['table'],
        'base field' => 'entity_id',
        'title' => t('CiviCRM Custom Field: @title', ['@title' => $field['group']]),
        'label' => t('CiviCRM Custom Field: @title', ['@title' => $field['group']]),
        'help' => t('CiviCRM Custom Field: @title', ['@title' => $field['group']]),
      ],
    ];

    $data[$field['table']][$field['name']] = [
      'title' => $field['title'],
      'help' => $field['title'],
      'real field' => $field['real field'],
      'field' => _civicrm_views_field_to_field_plugin($civicrm_entities[$field['join']], $field),
      'sort' => _civicrm_views_field_to_sort_plugin($civicrm_entities[$field['join']], $field),
      'filter' => _civicrm_views_field_to_filter_plugin($civicrm_entities[$field['join']], $field),
      'argument' => _civicrm_views_field_to_argument_plugin($civicrm_entity, $field),
      'relationship' => [],
    ];
  }

  return $data;
}

/**
 * Implements hook_views_data_alter().
 */
function civicrm_views_views_data_alter(&$data) {
  try {
    \Drupal::service('civicrm')->initialize();
  }
  catch (Exception $e) {
    \Drupal::logger('civicrm_views')
      ->error("Unable to initialize CiviCRM. Aborting hook_views_data_alter().");
    return;
  }

  // Only join to the users table if the civicrm tables are in the same
  // database.
  if (!Database::getConnectionInfo('civicrm')) {
    $data['users']['table']['join']['civicrm_uf_match'] = [
      'left_field' => 'uf_id',
      'field' => 'uid',
      'extra' => [
        [
          'table' => 'civicrm_uf_match',
          'field' => 'domain_id',
          'value' => \CRM_Core_Config::domainID(),
          'numeric' => TRUE,
        ],
      ],
    ];
  }
}

/**
 * Returns an array of CiviCRM entities.
 *
 * @return array
 *   Returns a list of all entities in CiviCRM.
 */
function _civicrm_views_civicrm_entities() {
  return [
    'civicrm_activity' => [
      'display_name' => t('CiviCRM Activity'),
      'dao' => 'CRM_Activity_DAO_Activity',
      'base' => TRUE,
      'custom_fields' => TRUE,
    ],
    'civicrm_address' => [
      'display_name' => t('CiviCRM Address'),
      'dao' => 'CRM_Core_DAO_Address',
      'base' => TRUE,
    ],
    'civicrm_contact' => [
      'display_name' => t('CiviCRM Contact'),
      'dao' => 'CRM_Contact_DAO_Contact',
      'base' => TRUE,
    ],
    'civicrm_contribution' => [
      'display_name' => t('CiviCRM Contribution'),
      'dao' => 'CRM_Contribute_DAO_Contribution',
      'base' => TRUE,
    ],
    'civicrm_email' => [
      'display_name' => t('CiviCRM Email'),
      'dao' => 'CRM_Core_DAO_Email',
      'base' => FALSE,
    ],
    'civicrm_entity_tag' => [
      'display_name' => t('CiviCRM Tag'),
      'dao' => 'CRM_Core_DAO_EntityTag',
      'base' => TRUE,
    ],
    'civicrm_event' => [
      'display_name' => 'CiviCRM Event',
      'dao' => 'CRM_Event_DAO_Event',
      'base' => TRUE,
    ],
    'civicrm_group' => [
      'display_name' => 'CiviCRM Group',
      'dao' => 'CRM_Contact_DAO_Group',
      'base' => TRUE,
    ],
    'civicrm_group_contact' => [
      'display_name' => 'CiviCRM Group Contact',
      'dao' => 'CRM_Contact_DAO_GroupContact',
      'base' => FALSE,
    ],
    'civicrm_membership' => [
      'display_name' => 'CiviCRM Membership',
      'dao' => 'CRM_Member_DAO_Membership',
      'base' => TRUE,
    ],
    'civicrm_membership_type' => [
      'display_name' => 'CiviCRM Membership Type',
      'dao' => 'CRM_Member_DAO_MembershipType',
      'base' => FALSE,
    ],
    'civicrm_participant' => [
      'display_name' => 'CiviCRM Participant',
      'dao' => 'CRM_Event_BAO_Participant',
      'base' => FALSE,
    ],
    'civicrm_phone' => [
      'display_name' => 'CiviCRM Phone',
      'dao' => 'CRM_Core_DAO_Phone',
      'base' => TRUE,
    ],
    'civicrm_relationship' => [
      'display_name' => 'CiviCRM Relationship',
      'dao' => 'CRM_Contact_DAO_Relationship',
      'base' => TRUE,
    ],
    'civicrm_relationship_type' => [
      'display_name' => 'CiviCRM Relationship Type',
      'dao' => 'CRM_Contact_DAO_RelationshipType',
      'base' => FALSE,
    ],
    'civicrm_uf_match' => [
      'display_name' => 'CiviCRM UF Match',
      'dao' => 'CRM_Core_DAO_UFMatch',
      'base' => FALSE,
    ],
  ];
}

/**
 * Adds relationships where needed.
 *
 * In general, we define a views relationship for one-to-many relationships.
 * A relationship may also be used in the case where an implicit join would be
 * rarely used or would bring in confusing or duplicate fields.
 *
 * This is ordered alphabetically by array key.
 */
function _civicrm_views_add_relationships(&$data) {
  foreach (\CRM_Activity_DAO_ActivityContact::buildOptions('record_type_id') as $record_type_id => $name) {
    $data['civicrm_activity']["id_contact_record_type_{$record_type_id}"]['relationship'] = [
      'id' => 'standard',
      'field' => 'id',
      'base' => 'civicrm_activity_contact',
      'base field' => 'activity_id',
      'extra' => [
        [
          'field' => 'record_type_id',
          'value' => $record_type_id,
        ],
      ],
      'title' => t('CiviCRM !record_type_name', ['!record_type_name' => $name]),
      'label' => t('CiviCRM !record_type_name (from Activity)', ['!record_type_name' => $name]),
      'help' => t('CiviCRM !record_type_name (from Activity)', ['!record_type_name' => $name]),
    ];
  }
  $data['civicrm_contact']['employer_id_contact']['relationship'] = [
    'id' => 'standard',
    'field' => 'employer_id',
    'base' => 'civicrm_contact',
    'base field' => 'id',
    'title' => t('CiviCRM Employer'),
    'label' => t('CiviCRM Employer (from Contact.employer_id'),
    'help' => t('CiviCRM Employer (from Contact.employer_id'),
  ];
  $data['civicrm_contact']['id_address']['relationship'] = [
    'id' => 'civicrm_location',
    'field' => 'id',
    'base' => 'civicrm_address',
    'base field' => 'contact_id',
    'title' => t('CiviCRM Addresses'),
    'label' => t('CiviCRM Addresses (from Contact.id)'),
    'help' => t('CiviCRM Addresses (from Contact.id)'),
  ];
  $data['civicrm_contact']['id_contribution']['relationship'] = [
    'id' => 'standard',
    'field' => 'id',
    'base' => 'civicrm_contribution',
    'base field' => 'contact_id',
    'title' => t('CiviCRM Contributions'),
    'label' => t('CiviCRM Contributions (from Contact.id)'),
    'help' => t('CiviCRM Contributions (from Contact.id)'),
  ];
  $data['civicrm_contact']['id_entity_tag']['relationship'] = [
    'id' => 'standard',
    'field' => 'id',
    'base' => 'civicrm_entity_tag',
    'base field' => 'entity_id',
    'extra' => [
      [
        'field' => 'entity_table',
        'value' => 'civicrm_contact',
      ],
    ],
    'title' => t('CiviCRM Tags'),
    'label' => t('CiviCRM Tags (from Contact.id)'),
    'help' => t('CiviCRM Tags (from Contact.id)'),
  ];
  $data['civicrm_contact']['id_email']['relationship'] = [
    'id' => 'civicrm_location',
    'field' => 'id',
    'base' => 'civicrm_email',
    'base field' => 'contact_id',
    'title' => t('CiviCRM Emails'),
    'label' => t('CiviCRM Emails (from Contact.id)'),
    'help' => t('CiviCRM Emails (from Contact.id)'),
  ];
  $data['civicrm_contact']['id_group_contact']['relationship'] = [
    'id' => 'standard',
    'field' => 'id',
    'base' => 'civicrm_group_contact',
    'base field' => 'contact_id',
    'title' => t('CiviCRM Groups'),
    'label' => t('CiviCRM Groups (from Contact.id)'),
    'help' => t('CiviCRM Groups (from Contact.id)'),
  ];
  $data['civicrm_contact']['id_membership']['relationship'] = [
    'id' => 'standard',
    'field' => 'id',
    'base' => 'civicrm_membership',
    'base field' => 'contact_id',
    'title' => t('CiviCRM Memberships'),
    'label' => t('CiviCRM Memberships (from Contact.id)'),
    'help' => t('CiviCRM Memberships (from Contact.id)'),
  ];
  $data['civicrm_contact']['id_participant']['relationship'] = [
    'id' => 'standard',
    'field' => 'id',
    'base' => 'civicrm_participant',
    'base field' => 'contact_id',
    'title' => t('CiviCRM Participants'),
    'label' => t('CiviCRM Participants (from Contact.id)'),
    'help' => t('CiviCRM Participants (from Contact.id)'),
  ];
  $data['civicrm_contact']['id_phone']['relationship'] = [
    'id' => 'civicrm_location',
    'field' => 'id',
    'base' => 'civicrm_phone',
    'base field' => 'contact_id',
    'title' => t('CiviCRM Phones'),
    'label' => t('CiviCRM Phones (from Contact.id)'),
    'help' => t('CiviCRM Phones (from Contact.id)'),
  ];
  $data['civicrm_contact']['id_relationship_a']['relationship'] = [
    'id' => 'civicrm_relationship',
    'field' => 'id',
    'base' => 'civicrm_relationship',
    'base field' => 'contact_id_a',
    'title' => t('CiviCRM Relationship as Contact A'),
    'label' => t('CiviCRM Relationship as Contact A (from Contact.id)'),
    'help' => t('CiviCRM Relationship as Contact A (from Contact.id)'),
  ];
  $data['civicrm_contact']['id_relationship_b']['relationship'] = [
    'id' => 'civicrm_relationship',
    'field' => 'id',
    'base' => 'civicrm_relationship',
    'base field' => 'contact_id_b',
    'title' => t('CiviCRM Relationship as Contact B'),
    'label' => t('CiviCRM Relationships as Contact B (from Contact.id)'),
    'help' => t('CiviCRM Relationships as Contact B (from Contact.id)'),
  ];
  $data['civicrm_contact']['id_uf_match']['relationship'] = [
    'id' => 'civicrm_uf_match',
    'field' => 'id',
    'base' => 'civicrm_uf_match',
    'base field' => 'contact_id',
    'title' => t('Drupal Users'),
    'label' => t('Drupal Users (from Contact.id)'),
    'help' => t('Drupal Users (from Contact.id)'),
  ];
  $data['civicrm_contact']['primary_contact_id_contact']['relationship'] = [
    'id' => 'standard',
    'field' => 'primary_contact_id',
    'base' => 'civicrm_contact',
    'base field' => 'id',
    'title' => t('CiviCRM Primary Contact'),
    'label' => t('CiviCRM Primary Contact (from Contact.primary_contact_id'),
    'help' => t('CiviCRM Primary Contact (from Contact.primary_contact_id'),
  ];
  $data['civicrm_entity_tag']['entity_id_contact']['relationship'] = [
    'id' => 'standard',
    'field' => 'entity_id',
    'base' => 'civicrm_contact',
    'base field' => 'id',
    'extra' => [
      [
        'table' => 'civicrm_entity_tag',
        'field' => 'entity_table',
        'value' => 'civicrm_contact',
      ],
    ],
    'title' => t('CiviCRM Contacts'),
    'label' => t('CiviCRM Contacts (from Tag.entity_id)'),
    'help' => t('CiviCRM Contacts (from Tag.entity_id)'),
  ];
  $data['civicrm_event']['id_participant']['relationship'] = [
    'id' => 'standard',
    'field' => 'id',
    'base' => 'civicrm_participant',
    'base field' => 'event_id',
    'title' => t('CiviCRM Participants'),
    'label' => t('CiviCRM Participants (from Event.id)'),
    'help' => t('CiviCRM Participants (from Event.id)'),
  ];
  $data['civicrm_group']['id_group_contact']['relationship'] = [
    'id' => 'standard',
    'field' => 'id',
    'base' => 'civicrm_group_contact',
    'base field' => 'group_id',
    'title' => t('CiviCRM Group Contacts'),
    'label' => t('CiviCRM Group Contacts (from Group.id)'),
    'help' => t('CiviCRM Group Contacts (from Group.id)'),
  ];
  $data['civicrm_relationship']['contact_id_a_contact']['relationship'] = [
    'id' => 'standard',
    'field' => 'contact_id_a',
    'base' => 'civicrm_contact',
    'base field' => 'id',
    'title' => t('CiviCRM Contact A'),
    'label' => t('CiviCRM Contact A (from Relationship.contact_id_a)'),
    'help' => t('CiviCRM Contact A (from Relationship.contact_id_a)'),
  ];
  $data['civicrm_relationship']['contact_id_b_contact']['relationship'] = [
    'id' => 'standard',
    'field' => 'contact_id_b',
    'base' => 'civicrm_contact',
    'base field' => 'id',
    'title' => t('CiviCRM Contact B'),
    'label' => t('CiviCRM Contact B (from Relationship.contact_id_b)'),
    'help' => t('CiviCRM Contact B (from Relationship.contact_id_b)'),
  ];
}

/**
 * Add implicit joins.
 *
 * We only define an implicit join in the case of a one-to-one relationship
 * and where the additional fields that the join makes visible are intuitively
 * understood.
 *
 * This is ordered alphabetically by array key.
 */
function _civicrm_views_add_implicit_joins(&$data) {
  $data['civicrm_address']['table']['join'] = [
    'civicrm_contribution' => [
      'left_field' => 'address_id',
      'field' => 'id',
    ],
    'civicrm_event' => [
      'left_field' => 'loc_block_id',
      'field' => 'id',
    ],
  ];
  $data['civicrm_contact']['table']['join'] = [
    'civicrm_activity_contact' => [
      'left_field' => 'contact_id',
      'field' => 'id',
    ],
    'civicrm_contribution' => [
      'left_field' => 'contact_id',
      'field' => 'id',
    ],
    'civicrm_group_contact' => [
      'left_field' => 'contact_id',
      'field' => 'id',
    ],
    'civicrm_membership' => [
      'left_field' => 'contact_id',
      'field' => 'id',
    ],
    'civicrm_participant' => [
      'left_field' => 'contact_id',
      'field' => 'id',
    ],
    'civicrm_uf_match' => [
      'left_field' => 'contact_id',
      'field' => 'id',
    ],
  ];
  $data['civicrm_group']['table']['join'] = [
    'civicrm_group_contact' => [
      'left_field' => 'group_id',
      'field' => 'id',
    ],
  ];
  $data['civicrm_event']['table']['join'] = [
    'civicrm_participant' => [
      'left_field' => 'event_id',
      'field' => 'id',
    ],
  ];
  $data['civicrm_membership_type']['table']['join'] = [
    'civicrm_membership' => [
      'left_field' => 'membership_type_id',
      'field' => 'id',
    ],
  ];
  $data['civicrm_relationship_type']['table']['join'] = [
    'civicrm_relationship' => [
      'left_field' => 'relationship_type_id',
      'field' => 'id',
    ],
  ];

  // Only join to the users table if the civicrm tables are in the same
  // database.
  if (!Database::getConnectionInfo('civicrm')) {
    $data['civicrm_uf_match']['table']['join'] = [
      'users' => [
        'left_field' => 'uid',
        'field' => 'uf_id',
        'extra' => [
          [
            'field' => 'domain_id',
            'value' => \CRM_Core_Config::domainID(),
            'numerical' => TRUE,
          ],
        ],
      ],
    ];
  }
}

/**
 * Map views fields to plugin types.
 *
 * T_INT        = 1,
 * T_STRING     = 2,
 * T_ENUM       = 2,
 * T_DATE       = 4,
 * T_TIME       = 8,
 * T_BOOLEAN    = 16,
 * T_TEXT       = 32,
 * T_LONGTEXT   = 32,
 * T_BLOB       = 64,
 * T_TIMESTAMP  = 256,
 * T_FLOAT      = 512,
 * T_MONEY      = 1024,
 * T_EMAIL      = 2048,
 * T_URL        = 4096,
 * T_CCNUM      = 8192,
 * T_MEDIUMBLOB = 16384;
 *
 * @Todo: display currencies nicely
 */
function _civicrm_views_field_to_field_plugin($entity, $field) {
  $type = !empty($field['pseudoconstant']) ? 'pseudoconstant' : $field['type'];

  switch ($type) {
    case CRM_Utils_Type::T_INT:
    case CRM_Utils_Type::T_FLOAT:
      return [
        'id' => 'numeric',
      ];

    case CRM_Utils_Type::T_ENUM:
    case CRM_Utils_Type::T_STRING:
      return [
        'id' => 'standard',
      ];

    case CRM_Utils_Type::T_TEXT:
    case CRM_Utils_Type::T_LONGTEXT:
      // @Todo: Override markup to allow configurable text formats
      return [
        'id' => 'civicrm_markup',
      ];

    case CRM_Utils_Type::T_BOOLEAN:
      return [
        'id' => 'boolean',
      ];

    case CRM_Utils_Type::T_URL:
      return [
        'id' => 'url',
      ];

    case 12:
    case CRM_Utils_Type::T_DATE:
    case CRM_Utils_Type::T_TIMESTAMP:
      return [
        'id' => 'civicrm_date',
      ];

    case 'pseudoconstant':
      return [
        'id' => 'civicrm_pseudoconstant',
        'pseudo callback' => "{$entity['dao']}::buildOptions",
        'pseudo arguments' => [$field['name']],
      ];

    default:
      return [
        'id' => 'standard',
      ];

  }
}

/**
 * Map views fields to sort plugin types.
 *
 * @see _civicrm_views_field_to_field_plugin
 *
 * @TODO: sorting for dates.
 */
function _civicrm_views_field_to_sort_plugin($entity, $field) {
  return [
    'id' => 'standard',
  ];
}

/**
 * Map views fields to filter plugin types.
 *
 * @see _civicrm_views_field_to_field_plugin
 */
function _civicrm_views_field_to_filter_plugin($entity, $field) {
  $type = !empty($field['pseudoconstant']) ? 'pseudoconstant' : $field['type'];

  switch ($type) {
    case CRM_Utils_Type::T_BOOLEAN:
      return [
        'id' => 'boolean',
      ];

    case CRM_Utils_Type::T_INT:
    case CRM_Utils_Type::T_FLOAT:
    case CRM_Utils_Type::T_MONEY:
      return [
        'id' => 'numeric',
      ];

    case CRM_Utils_Type::T_ENUM:
    case CRM_Utils_Type::T_STRING:
    case CRM_Utils_Type::T_TEXT:
    case CRM_Utils_Type::T_LONGTEXT:
    case CRM_Utils_Type::T_URL:
    case CRM_Utils_Type::T_EMAIL:
      return [
        'id' => 'string',
      ];

    case 12:
    case CRM_Utils_Type::T_DATE:
    case CRM_Utils_Type::T_TIMESTAMP:
      return [
        'id' => 'civicrm_date_operator',
      ];

    case 'pseudoconstant':
      return [
        'id' => 'civicrm_in_operator',
        'options callback' => "{$entity['dao']}::buildOptions",
        'options arguments' => [$field['name']],
      ];

    default:
      return [
        'id' => 'standard',
      ];
  }
}

/**
 * Map views fields to argument plugin types.
 *
 * For the meantime, only provide contextual filters for a limited number of
 * numeric and text fields.
 *
 * @see _civicrm_views_field_to_field_plugin
 */
function _civicrm_views_field_to_argument_plugin($entity, $field) {
  $type = !empty($field['pseudoconstant']) ? 'pseudoconstant' : $field['type'];

  switch ($type) {
    case CRM_Utils_Type::T_INT:
    case CRM_Utils_Type::T_FLOAT:
      return [
        'id' => 'numeric',
      ];

    case CRM_Utils_Type::T_ENUM:
    case CRM_Utils_Type::T_STRING:
    case CRM_Utils_Type::T_URL:
    case CRM_Utils_Type::T_EMAIL:
      return [
        'id' => 'string',
      ];

  }
}

/**
 * Pull in all the Custom fields.
 *
 * @return array
 *   An array of all custom fields.
 */
function _civicrm_views_get_custom_fields() {
  require_once 'api/v3/utils.php';
  $query = <<<SQL
    SELECT f.id, f.label, g.title, g.extends, f.data_type, f.column_name, f.option_group_id, g.table_name 
    FROM civicrm_custom_field f
    LEFT JOIN civicrm_custom_group g ON g.id = f.custom_group_id
    WHERE g.is_active = 1
SQL;

  $custom_fields = [];
  $dao = \CRM_Core_DAO::executeQuery($query);
  while ($dao->fetch()) {
    $custom_fields[] = [
      'title' => $dao->label,
      'group' => $dao->title,
      'table' => $dao->table_name,
      'name' => "custom_{$dao->id}",
      'real field' => $dao->column_name,
      'type' => CRM_Utils_Array::value($dao->data_type, CRM_Core_BAO_CustomField::dataToType()),
      'join' => _civicrm_views_get_join_table($dao->extends),
      'pseudoconstant' => $dao->option_group_id,
    ];
  }
  return $custom_fields;
}

/**
 * Returns the implicit join table.
 *
 * The table for a custom group is based on its entity type.
 *
 * @return string
 *   The table name for this entity type.
 */
function _civicrm_views_get_join_table($entity_type) {
  switch ($entity_type) {
    case "Contact":
    case "Individual":
    case "Household":
    case "Organization":
      return 'civicrm_contact';

    case "Group":
      return 'civicrm_group';

    case "Address":
      return 'civicrm_address';

    case "Event":
      return 'civicrm_event';

    case "Participant":
      return 'civicrm_participant';

    case "Contribution":
      return 'civicrm_contribution';

    case "Activity":
      return 'civicrm_activity';

    case "Relationship":
      return 'civicrm_relationship';

    case "Membership":
      return 'civicrm_membership';

    case "Grant":
      return 'civicrm_grant';

    case "Campaign":
    case "Survey":
      return 'civicrm_campaign';

    case "Case":
      return 'civicrm_case';

    default:
      return NULL;
  }
}
