<?php

/**
 * Implements hook_cron()
 */
function dosomething_rogue_cron() {
  dosomething_rogue_retry_failed_reportbacks();
}

function dosomething_rogue_retry_failed_reportbacks() {
  $task_log = db_select('dosomething_rogue_failed_task_log', 't')
    ->fields('t')
    ->execute()
    ->fetchAll();

  foreach ($task_log as $task) {
    if ($task->tries < 5) {
      if ($task->type === 'reportback') {
        // Check to see if the MIME type is missing
        if (strpos($task->file, 'data:;') !== false) {
          // Split file string to access the data
          $data = explode(',', $task->file)[1];

          // Decode and use getimagesizefromstring() to access the MIME type
          $image_size_info = getimagesizefromstring(base64_decode($data));
          $mimetype = $image_size_info['mime'];

          // Split the file string where the MIME type will go and rebuild to include MIME type
          $mime_split = explode(':', $task->file);
          $task->file = $mime_split[0] . ':' . $mimetype . $mime_split[1];
        }

        $data = (array)$task;

        if ($task->drupal_id) {
          $user = user_load($task->drupal_id);
        }
        elseif ($task->northstar_id) {
          $northstar_user = dosomething_northstar_get_user($task->northstar_id);
          $user = user_load($northstar_user->drupal_id);
        }
        else {
          dosomething_rogue_handle_failure($data, NULL, $task->id);
          continue;
        }

        $rogue_reportback = dosomething_rogue_send_reportback_to_rogue($data, $user, $task->id);

        if ($rogue_reportback) {
          dosomething_rogue_check_rbid_and_store_ref($rogue_reportback);

          db_delete('dosomething_rogue_failed_task_log')
            ->condition('id', $task->id)
            ->execute();
        }
      } elseif ($task->type === 'signup') {
        // Load the user
        $northstar_user = dosomething_northstar_get_user($task->northstar_id);
        $user = user_load($northstar_user->drupal_id);

        $rogue_signup = dosomething_rogue_send_signup_to_rogue((array)$task, $user, $task->id);

        if ($rogue_signup) {
          // Remove signup from failed log once it has been send successfully
          db_delete('dosomething_rogue_failed_task_log')
            ->condition('id', $task->id)
            ->execute();
        }
      } else {
        $data = [
          [
            'rogue_event_id' => $task->rogue_event_id,
            'status' => $task->status,
            'event_type' => 'review_photo',
          ]
        ];

        $response = dosomething_rogue_update_rogue_reportback_items($data, $task->id);

        if ($response) {
          db_delete('dosomething_rogue_failed_task_log')
            ->condition('id', $task->id)
            ->execute();
        }
      }
    }
  }
}
