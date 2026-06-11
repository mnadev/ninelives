module.exports = [
  {
    type: 'heading',
    defaultValue: 'Step or Ded'
  },
  {
    type: 'text',
    defaultValue: 'Keep your cat alive by hitting your daily step goal.'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Goals'
      },
      {
        type: 'input',
        messageKey: 'StepGoal',
        defaultValue: '0',
        label: 'Daily Step Goal',
        attributes: {
          placeholder: '0 = auto from Health',
          type: 'number',
          min: 0,
          max: 100000
        }
      },
      {
        type: 'select',
        messageKey: 'Difficulty',
        defaultValue: '1',
        label: 'Difficulty',
        options: [
          { value: '0', label: 'Forgiving (50% goal)' },
          { value: '1', label: 'Strict (exact goal)' }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Cat'
      },
      {
        type: 'select',
        messageKey: 'CatBreed',
        defaultValue: '0',
        label: 'Breed / Color',
        options: [
          { value: '0', label: 'Orange Tabby' },
          { value: '1', label: 'Tuxedo' },
          { value: '2', label: 'Siamese' },
          { value: '3', label: 'Void (Black)' }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Active Hours'
      },
      {
        type: 'select',
        messageKey: 'ActiveStart',
        defaultValue: '8',
        label: 'Start Hour',
        options: Array.from({ length: 24 }, (_, hour) => ({
          value: String(hour),
          label: `${hour}:00`
        }))
      },
      {
        type: 'select',
        messageKey: 'ActiveEnd',
        defaultValue: '22',
        label: 'End Hour',
        options: Array.from({ length: 24 }, (_, hour) => ({
          value: String(hour + 1),
          label: `${hour + 1}:00`
        }))
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save Settings'
  }
];
