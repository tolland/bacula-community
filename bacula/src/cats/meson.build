# Example: Process template files from src/cats
# Process .in template files using configure_file

# Process the create_bacula_database.in template
# Note: template_conf is defined in parent meson.build
create_bacula_database = configure_file(
  input: 'src/cats/create_bacula_database.in',
  output: 'create_bacula_database',
  configuration: template_conf,
)

# Example: You can process multiple templates similarly
# create_postgresql_database = configure_file(
#   input: 'src/cats/create_postgresql_database.in',
#   output: 'create_postgresql_database',
#   configuration: template_conf,
# )

