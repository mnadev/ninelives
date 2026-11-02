function formatSteps(steps) {
  return String(steps).replace(/\B(?=(\d{3})+(?!\d))/g, ',');
}

function goalOptions() {
  var options = [{ value: '0', label: 'Smart: adapts to you' }];
  var goals = [];
  for (var steps = 2000; steps <= 12000; steps += 500) {
    goals.push(steps);
  }
  goals.push(13000, 14000, 15000, 17500, 20000, 25000, 30000);
  goals.forEach(function(steps) {
    options.push({ value: String(steps), label: formatSteps(steps) + ' steps' });
  });
  return options;
}

function hourLabel(hour) {
  if (hour === 0 || hour === 24) {
    return '12 AM (midnight)';
  }
  if (hour === 12) {
    return '12 PM (noon)';
  }
  return (hour % 12) + (hour < 12 ? ' AM' : ' PM');
}

function hourOptions(first, last) {
  var options = [];
  for (var hour = first; hour <= last; hour++) {
    options.push({ value: String(hour), label: hourLabel(hour) });
  }
  return options;
}

module.exports = [
  {
    type: 'heading',
    defaultValue: 'NineLives'
  },
  {
    type: 'text',
    defaultValue: 'Keep your cats alive by walking. Each day you hit your goal, a new cat '
      + 'joins you (up to 3). Each day you miss it, one leaves. If they all go, walk '
      + '1,000 steps to bring one back.'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Goal'
      },
      {
        type: 'select',
        messageKey: 'StepGoal',
        defaultValue: '0',
        label: 'Daily step goal',
        description: 'Smart starts a little under your typical day, with weekends counted '
          + 'separately. Each day you hit it, the next asks 5% more; each miss eases it back.',
        options: goalOptions()
      },
      {
        type: 'select',
        messageKey: 'Difficulty',
        defaultValue: '1',
        label: 'Difficulty',
        options: [
          { value: '1', label: 'Strict: miss the goal, lose a cat' },
          { value: '0', label: 'Forgiving: half the goal keeps them safe' }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Cats'
      },
      {
        type: 'select',
        messageKey: 'CatBreed',
        defaultValue: '0',
        label: 'Your first cat',
        description: 'Cats you earn after that are the next breeds in the list.',
        options: [
          { value: '0', label: 'Orange tabby' },
          { value: '1', label: 'Tuxedo' },
          { value: '2', label: 'Siamese' },
          { value: '3', label: 'Void (black)' }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Active hours'
      },
      {
        type: 'text',
        defaultValue: 'Your cats expect your steps to be spread across these hours, and get '
          + 'restless when you fall behind. Before the first hour they just relax.'
      },
      {
        type: 'select',
        messageKey: 'ActiveStart',
        defaultValue: '8',
        label: 'From',
        options: hourOptions(0, 23)
      },
      {
        type: 'select',
        messageKey: 'ActiveEnd',
        defaultValue: '22',
        label: 'Until',
        options: hourOptions(1, 24)
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
