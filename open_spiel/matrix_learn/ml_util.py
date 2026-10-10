def get_cycle_bots(spec):
    """Spec is a list of tuples cycle, count."""
    bots = []
    for cycle, count in spec:
        for _ in range(count):
            bot = CycleBot(cycle)
            bots.append(bot)
    return bots


